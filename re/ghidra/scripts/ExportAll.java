// Export function list and decompiled C of the whole program.
// args: <outdir>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        new File(out).mkdirs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        PrintWriter tsv = new PrintWriter(new FileWriter(out + "/functions.tsv"));
        PrintWriter c = new PrintWriter(new FileWriter(out + "/decomp.c"));
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            tsv.printf("%s\t%d\t%s\n", f.getEntryPoint(), f.getBody().getNumAddresses(), f.getName());
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            c.printf("\n/* ==== %s @ %s ==== */\n", f.getName(), f.getEntryPoint());
            if (r != null && r.decompileCompleted())
                c.print(r.getDecompiledFunction().getC());
            else
                c.print("/* decompile failed */\n");
        }
        tsv.close(); c.close();
        // global data items with their inferred types (for struct recovery)
        PrintWriter dt = new PrintWriter(new FileWriter(out + "/data.tsv"));
        for (ghidra.program.model.mem.MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (b.getName().equals(".text")) continue;
            DataIterator di2 = currentProgram.getListing().getDefinedData(new ghidra.program.model.address.AddressSet(b.getStart(), b.getEnd()), true);
            while (di2.hasNext()) {
                Data d = di2.next();
                int refs = currentProgram.getReferenceManager().getReferenceCountTo(d.getAddress());
                dt.printf("%s\t%d\t%s\t%d\n", d.getAddress(), d.getLength(), d.getDataType().getName(), refs);
            }
        }
        dt.close();
        di.dispose();
    }
}
