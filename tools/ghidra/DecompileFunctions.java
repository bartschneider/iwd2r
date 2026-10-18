// Decompiles functions to one file each under <out dir>: the function named
// <name> is written to <out dir>/<name>.c.  Arguments are hex addresses such
// as 0x5845D0 (a function entry) or full names such as
// CGameSprite::MoveToPoint.
//@category iwd2r

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class DecompileFunctions extends GhidraScript {

    private static final int TIMEOUT_SECONDS = 60;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException(
                "usage: DecompileFunctions.java <out dir> <name or address>...");
        }
        File dir = new File(args[0]);
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IllegalArgumentException("cannot create " + args[0]);
        }
        DecompInterface decomp = new DecompInterface();
        try {
            decomp.setOptions(new DecompileOptions());
            decomp.toggleCCode(true);
            decomp.toggleSyntaxTree(true);
            decomp.setSimplificationStyle("decompile");
            boolean opened = decomp.openProgram(currentProgram);
            for (int i = 1; i < args.length; i++) {
                List<Function> found = findFunctions(args[i]);
                if (found.size() == 1) {
                    write(dir, args[i], found.get(0),
                        opened ? decomp : null, decomp.getLastMessage());
                }
                else {
                    println("DECOMPILE " + args[i] + "\t" + (found.isEmpty()
                        ? "not found" : found.size() + " matches"));
                }
            }
        }
        finally {
            decomp.dispose();
        }
    }

    private void write(File dir, String query, Function func,
            DecompInterface decomp, String openError) throws Exception {
        String code = decomp == null
            ? "// decompiler failed to open the program: " + openError + "\n"
            : decompile(decomp, func);
        File out = new File(dir, func.getName(true) + ".c");
        try (PrintWriter pw = new PrintWriter(out, "UTF-8")) {
            pw.print(code.endsWith("\n") ? code : code + "\n");
        }
        println("DECOMPILE " + query + "\t" + hex(func.getEntryPoint()) + "\t"
            + func.getName(true) + "\t" + out.getPath());
    }

    private List<Function> findFunctions(String query) {
        List<Function> result = new ArrayList<>();
        if (query.matches("(?i)0x[0-9a-f]+")) {
            Function func = getFunctionAt(toAddr(Long.parseLong(query.substring(2), 16)));
            if (func != null) {
                result.add(func);
            }
            return result;
        }
        int split = query.lastIndexOf("::");
        String simple = split < 0 ? query : query.substring(split + 2);
        Set<Address> seen = new TreeSet<>();
        SymbolIterator it = currentProgram.getSymbolTable().getSymbols(simple);
        while (it.hasNext()) {
            Symbol sym = it.next();
            Function func = getFunctionAt(sym.getAddress());
            if (sym.getName(true).equals(query) && !sym.isExternal() && func != null
                && seen.add(func.getEntryPoint())) {
                result.add(func);
            }
        }
        return result;
    }

    private String decompile(DecompInterface decomp, Function func) {
        DecompileResults res = decomp.decompileFunction(func, TIMEOUT_SECONDS, monitor);
        if (!res.decompileCompleted() || res.getDecompiledFunction() == null) {
            return "// decompilation failed: " + res.getErrorMessage() + "\n";
        }
        return res.getDecompiledFunction().getC();
    }

    private static String hex(Address addr) {
        return "0x" + Long.toHexString(addr.getOffset()).toUpperCase();
    }
}
