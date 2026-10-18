// Lists the current program's functions that have no counterpart in another
// program: a counterpart has the same referenced string literals, the same
// imported (external) calls, and an instruction size within 25%.  Writes one
// tab-separated "address	name	size	strings" row per remaining function to
// <out.tsv>:  -postScript DllDelta.java <other .gpr> <other program> <out.tsv>
//@category iwd2r

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

import ghidra.app.script.GhidraScript;
import ghidra.base.project.GhidraProject;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Program;
import ghidra.program.model.symbol.Reference;

public class DllDelta extends GhidraScript {

    private Path tempDir;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: DllDelta.java <other .gpr> <other program name> <out.tsv>");
        }
        try {
            Path gpr = new File(args[0]).getAbsoluteFile().toPath().normalize();
            String otherName = gpr.getFileName().toString().replaceFirst("\\.gpr$", "");
            GhidraProject other = GhidraProject.openProject(
                reach(gpr.getParent()).toString(), otherName, false);
            try {
                Program theirs = other.openProgram("/", args[1], true);
                Map<String, List<Long>> seen = new HashMap<>();
                for (Function f : theirs.getFunctionManager().getFunctions(true)) {
                    Signature s = signature(theirs, f);
                    seen.computeIfAbsent(s.key, k -> new ArrayList<>()).add(s.size);
                }
                int rows = 0;
                try (PrintWriter out = new PrintWriter(args[2], "UTF-8")) {
                    for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                        Signature s = signature(currentProgram, f);
                        if (hasCounterpart(s, seen)) {
                            continue;
                        }
                        rows++;
                        out.println(hex(f.getEntryPoint()) + "\t" + f.getName(true) + "\t"
                            + f.getBody().getNumAddresses() + "\t"
                            + String.join(",", s.strings));
                    }
                }
                println("DLLDELTA " + rows
                    + " functions with no counterpart, written to " + args[2]);
            }
            finally {
                other.close();
            }
        }
        finally {
            cleanup();
        }
    }

    // Ghidra refuses project paths with an element starting with '.' (ours
    // live under ~/.local/...), so reach such a project through a dot-free
    // symlink, the way make_project.headless runs headless itself.
    private Path reach(Path dir) throws IOException {
        for (Path element : dir) {
            if (element.toString().startsWith(".")) {
                tempDir = Files.createTempDirectory("iwd2r-delta-");
                Path link = tempDir.resolve(dir.getFileName().toString());
                Files.createSymbolicLink(link, dir.toAbsolutePath());
                return link;
            }
        }
        return dir;
    }

    private void cleanup() throws IOException {
        if (tempDir == null) {
            return;
        }
        try (var walk = Files.walk(tempDir)) {
            walk.sorted(Comparator.reverseOrder()).forEach(p -> p.toFile().delete());
        }
    }

    private boolean hasCounterpart(Signature s, Map<String, List<Long>> seen) {
        for (long size : seen.getOrDefault(s.key, new ArrayList<>())) {
            if (Math.abs(size - s.size) * 4 <= s.size) {  // within 25%
                return true;
            }
        }
        return false;
    }

    // A function's counterpart key: its referenced string literals and the
    // external functions it calls (string runs flattened so a row stays on
    // one line).
    private Signature signature(Program program, Function f) {
        Signature s = new Signature();
        TreeSet<String> imports = new TreeSet<>();
        AddressSetView body = f.getBody();
        InstructionIterator instructions = program.getListing().getInstructions(body, true);
        while (instructions.hasNext()) {
            Instruction i = instructions.next();
            for (Reference r : i.getReferencesFrom()) {
                Data data = program.getListing().getDataAt(r.getToAddress());
                if (data != null && data.hasStringValue()) {
                    s.strings.add(((String) data.getValue()).replaceAll("\\s+", " "));
                    continue;
                }
                Function callee = program.getFunctionManager().getFunctionAt(r.getToAddress());
                if (callee != null && callee.isExternal()) {
                    imports.add(callee.getName());
                }
            }
        }
        s.key = "strings: " + String.join("\u0000", s.strings)
            + "\nimports: " + String.join("\u0000", imports);
        s.size = body.getNumAddresses();
        return s;
    }

    private static String hex(Address addr) {
        return "0x" + Long.toHexString(addr.getOffset()).toUpperCase();
    }

    private static final class Signature {
        String key;
        long size;
        TreeSet<String> strings = new TreeSet<>();
    }
}
