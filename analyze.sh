#!/bin/bash
# Rodent's Revenge static recompilation - Ghidra headless analysis
# Uses AnalyzeWin16.java from recomp-tools to analyze 16-bit NE binaries

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RECOMP_TOOLS="/home/christopher/Documents/Development/recomp-tools"
GHIDRA_HOME="/snap/ghidra/35/ghidra_12.0_PUBLIC"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"

GAME_DIR="$SCRIPT_DIR/rodents_revenge"
PROJECT_DIR="$SCRIPT_DIR/ghidra_project"
DECOMPILED_DIR="$SCRIPT_DIR/decompiled"

# Ensure output dirs exist
mkdir -p "$DECOMPILED_DIR/field100/ghidra_out"
mkdir -p "$DECOMPILED_DIR/rodent/ghidra_out"
mkdir -p "$PROJECT_DIR"

usage() {
    echo "Usage: $0 [field100|rodent|all]"
    echo ""
    echo "  field100  - Analyze field100.dll (game logic - recommended first)"
    echo "  rodent    - Analyze rodent.exe (VB1 form shell)"
    echo "  all       - Analyze both"
    echo ""
    echo "  --reprocess  - Re-run script without re-analyzing (faster)"
    exit 1
}

analyze_binary() {
    local name="$1"
    local binary="$2"
    local out_dir="$3"
    local reprocess="${4:-false}"

    echo "=== Analyzing $name: $binary ==="
    echo "Output: $out_dir"

    if [ "$reprocess" = "true" ] && [ -d "$PROJECT_DIR/RodentRecomp.rep" ]; then
        echo "Re-processing (no re-analysis)..."
        "$ANALYZE" \
            "$PROJECT_DIR" RodentRecomp \
            -process "$(basename "$binary")" \
            -noanalysis \
            -scriptPath "$RECOMP_TOOLS/ghidra" \
            -postScript AnalyzeWin16.java "$out_dir"
    else
        echo "Importing and analyzing..."
        "$ANALYZE" \
            "$PROJECT_DIR" RodentRecomp \
            -import "$binary" \
            -scriptPath "$RECOMP_TOOLS/ghidra" \
            -postScript AnalyzeWin16.java "$out_dir"
    fi

    echo "=== Done: $name ==="
    echo ""
}

# Parse args
TARGET="${1:-all}"
REPROCESS=false
for arg in "$@"; do
    if [ "$arg" = "--reprocess" ]; then
        REPROCESS=true
    fi
done

case "$TARGET" in
    field100)
        analyze_binary "field100.dll" "$GAME_DIR/field100.dll" "$DECOMPILED_DIR/field100/ghidra_out" "$REPROCESS"
        ;;
    rodent)
        analyze_binary "rodent.exe" "$GAME_DIR/rodent.exe" "$DECOMPILED_DIR/rodent/ghidra_out" "$REPROCESS"
        ;;
    all)
        # field100.dll first - it has the real game logic
        analyze_binary "field100.dll" "$GAME_DIR/field100.dll" "$DECOMPILED_DIR/field100/ghidra_out" "$REPROCESS"
        analyze_binary "rodent.exe" "$GAME_DIR/rodent.exe" "$DECOMPILED_DIR/rodent/ghidra_out" "$REPROCESS"
        ;;
    --help|-h)
        usage
        ;;
    *)
        echo "Unknown target: $TARGET"
        usage
        ;;
esac

echo "Analysis complete. Results in: $DECOMPILED_DIR/"
echo ""
echo "Next steps:"
echo "  cat $DECOMPILED_DIR/field100/ghidra_out/summary.txt"
echo "  cat $DECOMPILED_DIR/field100/ghidra_out/exports.txt"
echo "  cat $DECOMPILED_DIR/field100/ghidra_out/decompiled.c"
