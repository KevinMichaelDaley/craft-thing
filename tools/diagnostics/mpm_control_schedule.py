"""Expose valid workgroup scheduling for the production convergence update."""
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text()
if source.count("void main()") != 1:
    raise SystemExit("expected exactly one production entry point")
source = source.replace("void main()", "void production_main()")
source = source.replace("buffer Activity", "coherent buffer Activity")
source += """
void main() {
    if (gl_LocalInvocationIndex == 0u) production_main();
    memoryBarrierBuffer();
    barrier();
    if (gl_LocalInvocationIndex != 0u) production_main();
}
"""
pathlib.Path(sys.argv[2]).write_text(source)
