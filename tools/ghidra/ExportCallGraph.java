// Exports the whole-program function table and call graph as two TSVs:
//   functions.tsv : address \t name \t size   (hex address, decimal byte size)
//   callgraph.tsv : address \t callee...      (hex addresses, one line per fn)
// No header line: tools/parity/run.py parses both with
// address_markers.read_functions / read_callgraph, which int()-parse field 0.
// Only DIRECT calls produce edges -- computed and virtual dispatch have no
// single static target and are invisible in callgraph.tsv.
// Usage: ExportCallGraph.java <out dir>
//@category iwd2r

import java.io.File;
import java.io.PrintWriter;
import java.util.Set;
import java.util.TreeSet;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.FlowType;

public class ExportCallGraph extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: ExportCallGraph.java <out dir>");
        }
        File dir = new File(args[0]);
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IllegalArgumentException("cannot create " + args[0]);
        }
        File functions = new File(dir, "functions.tsv");
        File callgraph = new File(dir, "callgraph.tsv");
        int n = 0;
        int edges = 0;
        try (PrintWriter fns = new PrintWriter(functions, "UTF-8");
             PrintWriter calls = new PrintWriter(callgraph, "UTF-8")) {
            for (Function func : currentProgram.getFunctionManager().getFunctions(true)) {
                if (func.isExternal()) {
                    continue;
                }
                Set<Address> callees = callees(func);
                fns.printf("%X\t%s\t%d\n", func.getEntryPoint().getOffset(),
                    func.getName(true), func.getBody().getNumAddresses());
                calls.print(Long.toHexString(func.getEntryPoint().getOffset())
                    .toUpperCase());
                for (Address to : callees) {
                    calls.print("\t" + Long.toHexString(to.getOffset()).toUpperCase());
                    edges++;
                }
                calls.print("\n");
                n++;
            }
        }
        println("EXPORTCALLGRAPH\t" + n + " functions\t" + edges + " direct edges\t"
            + functions.getPath());
    }

    /** Every address this function reaches by DIRECT call (call flow targets). */
    private Set<Address> callees(Function func) {
        Set<Address> out = new TreeSet<>();
        InstructionIterator it =
            currentProgram.getListing().getInstructions(func.getBody(), true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            FlowType flow = ins.getFlowType();
            if (!flow.isCall()) {
                continue;
            }
            for (Address to : ins.getFlows()) {
                out.add(to);
            }
        }
        return out;
    }
}
