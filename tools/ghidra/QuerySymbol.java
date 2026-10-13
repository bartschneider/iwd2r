// iwd2r. Distributed under LICENSE.md.
// Prints, for each argument (a hex address such as 0x5845D0, or a name such as
// IcewindMisc::Roll), one line per match:
//   QUERY <arg>\t0xADDR\t<full name>\tfunction=yes|no
// or "QUERY <arg>\tnot found".
//@category iwd2r

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class QuerySymbol extends GhidraScript {

    @Override
    protected void run() throws Exception {
        for (String arg : getScriptArgs()) {
            boolean found = false;
            if (arg.matches("(?i)0x[0-9a-f]+")) {
                Address addr = toAddr(Long.parseLong(arg.substring(2), 16));
                Function func = getFunctionAt(addr);
                Symbol sym = func != null ? func.getSymbol() : getSymbolAt(addr);
                print(arg, addr, sym != null ? sym.getName(true) : "<no symbol>", func != null);
                found = true;
            }
            else {
                int split = arg.lastIndexOf("::");
                String simple = split < 0 ? arg : arg.substring(split + 2);
                SymbolIterator it = currentProgram.getSymbolTable().getSymbols(simple);
                while (it.hasNext()) {
                    Symbol sym = it.next();
                    if (sym.getName(true).equals(arg) && !sym.isExternal()) {
                        print(arg, sym.getAddress(), sym.getName(true),
                            getFunctionAt(sym.getAddress()) != null);
                        found = true;
                    }
                }
            }
            if (!found) {
                println("QUERY " + arg + "\tnot found");
            }
        }
    }

    private void print(String arg, Address addr, String name, boolean isFunction) {
        println("QUERY " + arg + "\t0x" + addr.toString().toUpperCase() + "\t" + name
            + "\tfunction=" + (isFunction ? "yes" : "no"));
    }
}
