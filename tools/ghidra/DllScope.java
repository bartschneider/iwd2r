// Prints the size and import/export surface of the current program (a DLL):
//   DLLSCOPE functions=N code_bytes=N imports=N gl_imports=N exports=N
//   ieex_helper_exports=N
// followed by one line each for the imported libraries, the gl*/wgl* imports
// and the IEex_Helper_* exports that look render-related.
//@category iwd2r

import java.util.TreeSet;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

public class DllScope extends GhidraScript {

    // From the fork's own notes: the names of its rendering helpers carry
    // one of these marks.
    private static final String RENDER_RE =
        ".*(Tile|Fog|Present|Lightning|Hud|HUD|Fill|Atlas|Scaled|Dialog|Gl|GL|Render|Zoom).*";

    @Override
    protected void run() throws Exception {
        FunctionManager functions = currentProgram.getFunctionManager();
        long count = 0, bytes = 0;
        for (Function f : functions.getFunctions(true)) {
            count++;
            bytes += f.getBody().getNumAddresses();
        }
        SymbolTable symbols = currentProgram.getSymbolTable();
        TreeSet<String> gl = new TreeSet<>();
        TreeSet<String> libs = new TreeSet<>();
        int imports = 0;
        for (Symbol s : symbols.getExternalSymbols()) {
            imports++;
            if (s.getName().startsWith("gl") || s.getName().startsWith("wgl")) {
                gl.add(s.getName());
            }
            libs.add(s.getParentNamespace().getName());
        }
        TreeSet<String> helpers = new TreeSet<>();
        int exports = 0;
        for (Symbol s : symbols.getAllSymbols(true)) {
            if (!s.isExternalEntryPoint()) {
                continue;
            }
            exports++;
            if (s.getName().startsWith("IEex_Helper_")) {
                helpers.add(s.getName());
            }
        }
        StringBuilder render = new StringBuilder();
        for (String h : helpers) {
            if (h.matches(RENDER_RE)) {
                render.append(h).append(' ');
            }
        }
        println("DLLSCOPE functions=" + count + " code_bytes=" + bytes
            + " imports=" + imports + " gl_imports=" + gl.size()
            + " exports=" + exports
            + " ieex_helper_exports=" + helpers.size());
        println("DLLSCOPE libs: " + String.join(" ", libs));
        println("DLLSCOPE gl: " + String.join(" ", gl));
        println("DLLSCOPE render_helpers: " + render);
    }
}
