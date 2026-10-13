// Applies function names from a labels file written by make_project.py.
// Arguments: <labels.tsv> <summary.txt>. Each labels line is
// "ADDR<TAB>SOURCE<TAB>NAME" with ADDR in hex and SOURCE "ours" or "ieex".
// A function is created at ADDR if none exists; NAME ("A::B::f") becomes the
// function's primary name inside namespace A::B. Safe to run before analysis
// (as a pre-script), after it, or again on an existing project.
//@category iwd2r

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class ImportLabels extends GhidraScript {

    private static class Tally {
        int named, unchanged, created, skippedThunk, failed;
    }

    private final List<String> renamed = new ArrayList<>();

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("usage: ImportLabels.java <labels.tsv> <summary.txt>");
        }
        Tally ours = new Tally();
        Tally ieex = new Tally();
        List<String> failures = new ArrayList<>();
        renamed.clear();
        try (BufferedReader in = new BufferedReader(new FileReader(args[0]))) {
            String line;
            while ((line = in.readLine()) != null) {
                if (monitor.isCancelled()) {
                    break;
                }
                String[] f = line.split("\t", 3);
                if (f.length != 3) {
                    continue;
                }
                Tally t = f[1].equals("ours") ? ours : ieex;
                SourceType source = f[1].equals("ours") ? SourceType.USER_DEFINED : SourceType.IMPORTED;
                String error = apply(Long.parseLong(f[0], 16), f[2], source, t);
                if (error != null) {
                    t.failed++;
                    failures.add("0x" + f[0].toUpperCase() + "\t" + f[1] + "\t" + f[2] + "\t" + error);
                }
            }
        }
        try (PrintWriter out = new PrintWriter(args[1])) {
            report(out, "ours", ours);
            report(out, "ieex", ieex);
            for (String r : renamed) {
                out.println("renamed\t" + r);
            }
            for (String failure : failures) {
                out.println("failure\t" + failure);
            }
        }
        println("ours: " + summary(ours));
        println("ieex: " + summary(ieex));
    }

    private static String summary(Tally t) {
        return "named=" + t.named + " unchanged=" + t.unchanged + " created=" + t.created
            + " skipped_thunk=" + t.skippedThunk + " failed=" + t.failed;
    }

    private static void report(PrintWriter out, String source, Tally t) {
        out.println(source + "\t" + summary(t));
    }

    private String apply(long offset, String fullName, SourceType source, Tally t) {
        Address addr = toAddr(offset);
        if (!currentProgram.getMemory().contains(addr)) {
            return "address not in memory";
        }
        try {
            Function func = getFunctionAt(addr);
            if (func == null) {
                if (getInstructionAt(addr) == null) {
                    if (!disassemble(addr)) {
                        return "disassembly failed";
                    }
                }
                func = createFunction(addr, null);
                if (func == null) {
                    return "createFunction failed";
                }
                t.created++;
            }
            if (func.isThunk() && func.getThunkedFunction(true).isExternal()) {
                t.skippedThunk++;
                return null;
            }
            String name = fullName.replaceAll("\\s+", "_");
            int split = name.lastIndexOf("::");
            String simple = split < 0 ? name : name.substring(split + 2);
            Namespace ns = split < 0 ? currentProgram.getGlobalNamespace()
                : NamespaceUtils.createNamespaceHierarchy(name.substring(0, split),
                    currentProgram.getGlobalNamespace(), currentProgram, source);
            // Equal name and namespace counts as applied whatever the source:
            // a thunk named like its default keeps SourceType.DEFAULT.
            if (func.getName().equals(simple) && func.getParentNamespace().equals(ns)) {
                t.unchanged++;
                return null;
            }
            // A label of the same name at this address (left behind when
            // analysis renamed the function) blocks the rename; drop it.
            for (Symbol sym : currentProgram.getSymbolTable().getSymbols(addr)) {
                if (!sym.equals(func.getSymbol()) && sym.getName().equals(simple)
                    && sym.getParentNamespace().equals(ns)) {
                    sym.delete();
                }
            }
            String before = func.getSymbol().getName(true) + " (" + func.getSymbol().getSource() + ")";
            func.getSymbol().setNameAndNamespace(simple, ns, source);
            t.named++;
            renamed.add("0x" + addr.toString().toUpperCase() + "\t" + before + " -> " + func.getSymbol().getName(true));
            return null;
        }
        catch (Exception e) {
            return e.getClass().getSimpleName() + ": " + e.getMessage();
        }
    }
}
