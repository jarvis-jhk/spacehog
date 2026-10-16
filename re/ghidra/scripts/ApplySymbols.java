// Create missing functions at "push ebp; mov ebp,esp" prologues inside undefined
// gaps, then apply names/prototypes from re/symbols.tsv (addr<TAB>name[<TAB>prototype]).
// args: <symbols.tsv>
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.util.parser.FunctionSignatureParser;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import ghidra.app.util.cparser.C.CParserUtils;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class ApplySymbols extends GhidraScript {
    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();
        MemoryBlock text = mem.getBlock(".text");
        int created = 0;
        for (int pass = 0; pass < 3; pass++) {
            Address a = text.getStart();
            while (a.compareTo(text.getEnd()) < 0) {
                if (listing.getInstructionContaining(a) == null && listing.getDefinedDataContaining(a) == null
                        && getFunctionContaining(a) == null) {
                    byte[] b = new byte[3];
                    try { mem.getBytes(a, b); } catch (Exception e) { break; }
                    if ((b[0] & 0xff) == 0x55 && (b[1] & 0xff) == 0x89 && (b[2] & 0xff) == 0xe5) {
                        disassemble(a);
                        if (createFunction(a, null) != null) created++;
                    }
                }
                a = a.add(1);
            }
            analyzeChanges(currentProgram);
        }
        println("created functions: " + created);
        String[] args = getScriptArgs();
        if (args.length == 0) return;
        if (args.length > 2) {
            // mark interrupt-modified globals volatile (own memory blocks)
            for (String line : Files.readAllLines(Paths.get(args[2]))) {
                if (line.isBlank() || line.startsWith("#")) continue;
                String[] f = line.split("\t");
                Address a = toAddr(Long.parseLong(f[0], 16));
                long len = Long.decode(f[1]);
                Address e = a.add(len);
                MemoryBlock b = mem.getBlock(a);
                if (b == null) continue;
                if (!b.getStart().equals(a)) { mem.split(b, a); b = mem.getBlock(a); }
                if (b.getEnd().compareTo(e) >= 0 && !b.getEnd().equals(e.subtract(1))) { mem.split(b, e); b = mem.getBlock(a); }
                b.setVolatile(true);
                println("volatile: " + a + " len " + len + " block " + b.getName());
            }
        }
        if (args.length > 1) {
            ghidra.app.util.cparser.C.CParser cp = new ghidra.app.util.cparser.C.CParser(currentProgram.getDataTypeManager(), true, null);
            cp.parse(new String(Files.readAllBytes(Paths.get(args[1]))));
            println("parsed types from " + args[1]);
        }
        int named = 0;
        for (String line : Files.readAllLines(Paths.get(args[0]))) {
            if (line.isBlank() || line.startsWith("#")) continue;
            String[] f = line.split("\t");
            Address addr = toAddr(Long.parseLong(f[0], 16));
            String name = f[1];
            Function fn = getFunctionAt(addr);
            if (fn == null && mem.getBlock(addr) == text) {
                disassemble(addr);
                fn = createFunction(addr, name);
            }
            if (fn != null) {
                fn.setName(name, SourceType.USER_DEFINED);
                if (f.length > 2 && !f[2].isBlank()) {
                    try {
                        FunctionSignatureParser p = new FunctionSignatureParser(currentProgram.getDataTypeManager(), null);
                        FunctionDefinitionDataType sig = p.parse(fn.getSignature(), f[2]);
                        new ghidra.app.cmd.function.ApplyFunctionSignatureCmd(addr, sig, SourceType.USER_DEFINED).applyTo(currentProgram);
                    } catch (Exception e) { println("bad prototype for " + name + ": " + e.getMessage()); }
                }
            } else {
                // data label, optionally with a data type ("TYPE" or "TYPE[n]")
                if (f.length > 2 && !f[2].isBlank()) {
                    try {
                        ghidra.util.data.DataTypeParser dp = new ghidra.util.data.DataTypeParser(currentProgram.getDataTypeManager(), currentProgram.getDataTypeManager(), null, ghidra.util.data.DataTypeParser.AllowedDataTypes.ALL);
                        DataType t = dp.parse(f[2].trim());
                        listing.clearCodeUnits(addr, addr.add(t.getLength() - 1), false);
                        listing.createData(addr, t);
                    } catch (Exception e) { println("bad data type for " + name + ": " + e.getMessage()); }
                }
                SymbolTable st = currentProgram.getSymbolTable();
                for (Symbol s : st.getSymbols(addr)) if (s.getSource() == SourceType.USER_DEFINED) s.delete();
                st.createLabel(addr, name, SourceType.USER_DEFINED).setPrimary();
            }
            named++;
        }
        println("applied symbols: " + named);
    }
}
