// iwd2r. Distributed under LICENSE.md.
// Decompiles one function for tools/oracle.py evidence.
// Arguments: <query> <output file>. The query is a hex address such as
// 0x5845D0 (a function entry) or a full name such as IcewindMisc::Roll.
// The output file is tab-separated, one record per line (addresses in the
// `// 0xADDR` form of src, upper-case hex without leading zeros):
//   match\t0xADDR\tNAME            each function the query names, when there
//                                  are several (nothing else follows)
//   function\t0xADDR\tNAME         the function
//   caller\t0xADDR\tNAME\t0xFROM   a reference to the entry from inside a
//                                  function (<none>\t<data> outside one)
//   callee\t0xADDR\tNAME           a function it calls
//   code                           then Ghidra's C output to the end
// No function record means the query matched nothing.
//@category iwd2r

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class DecompileToEvidence extends GhidraScript {

    private static final int TIMEOUT_SECONDS = 60;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("usage: DecompileToEvidence.java <query> <output>");
        }
        List<Function> found = findFunctions(args[0]);
        try (PrintWriter out = new PrintWriter(args[1], "UTF-8")) {
            if (found.size() > 1) {
                for (Function f : found) {
                    out.println("match\t" + hex(f.getEntryPoint()) + "\t" + f.getName(true));
                }
                return;
            }
            if (found.isEmpty()) {
                return;
            }
            Function func = found.get(0);
            out.println("function\t" + hex(func.getEntryPoint()) + "\t" + func.getName(true));
            for (Reference ref : getReferencesTo(func.getEntryPoint())) {
                Function caller = getFunctionContaining(ref.getFromAddress());
                if (caller != null) {
                    out.println("caller\t" + hex(caller.getEntryPoint()) + "\t" + caller.getName(true)
                        + "\t" + hex(ref.getFromAddress()));
                }
                else {
                    out.println("caller\t<none>\t<data>\t" + hex(ref.getFromAddress()));
                }
            }
            List<Function> callees = new ArrayList<>(func.getCalledFunctions(monitor));
            callees.sort(Comparator.comparing(Function::getEntryPoint));
            for (Function callee : callees) {
                out.println("callee\t" + hex(callee.getEntryPoint()) + "\t" + callee.getName(true));
            }
            out.println("code");
            out.print(decompile(func));
        }
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

    private String decompile(Function func) {
        DecompInterface decomp = new DecompInterface();
        try {
            decomp.setOptions(new DecompileOptions());
            decomp.toggleCCode(true);
            decomp.toggleSyntaxTree(true);
            decomp.setSimplificationStyle("decompile");
            if (!decomp.openProgram(currentProgram)) {
                return "// decompiler failed to open the program: " + decomp.getLastMessage() + "\n";
            }
            DecompileResults res = decomp.decompileFunction(func, TIMEOUT_SECONDS, monitor);
            if (!res.decompileCompleted() || res.getDecompiledFunction() == null) {
                return "// decompilation failed: " + res.getErrorMessage() + "\n";
            }
            return res.getDecompiledFunction().getC();
        }
        finally {
            decomp.dispose();
        }
    }

    private static String hex(Address addr) {
        return "0x" + Long.toHexString(addr.getOffset()).toUpperCase();
    }
}
