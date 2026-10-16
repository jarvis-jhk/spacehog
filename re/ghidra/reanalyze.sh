#!/bin/bash
# Re-apply re/symbols.tsv to the Ghidra project and re-export decompilation to re/ghidra/out.
set -e
source "$(dirname "$0")/../../tools/env.sh"
cd "$SH_ROOT"
tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless re/ghidra/proj spacehog -process SH.coff -noanalysis \
  -scriptPath re/ghidra/scripts -preScript ApplySymbols.java "$SH_ROOT/re/symbols.tsv" "$SH_ROOT/re/ghidra/types.h" "$SH_ROOT/re/volatile.tsv" \
  -postScript ExportAll.java "$SH_ROOT/re/ghidra/out" > scratch/ghidra-re.log 2>&1 || { tail -20 scratch/ghidra-re.log; exit 1; }
grep -E "created functions|applied symbols|bad prototype|parsed types|volatile|ERROR" scratch/ghidra-re.log | sed 's/.*> //'
