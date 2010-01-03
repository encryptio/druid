import os
env = Environment(CCFLAGS = '-O2 -Wall -std=c99 -Isrc')

def runTest(env,target,source):
    import subprocess
    app = str(source[0].abspath)
    if not subprocess.call(app):
        open(str(target[0]),'w').write("PASSED\n")

env.Object( 'obj/galois_field.o', 'src/galois_field.c' )
env.Object( 'obj/reed_solomon.o', 'src/reed_solomon.c' )

env.Object( 'obj/tests/gf_arithmetic.o', 'src/tests/gf_arithmetic.c' )
env.Object( 'obj/tests/rs.o', 'src/tests/rs.c' )
env.Object( 'obj/tests/test.o', 'src/tests/test.c' )

env.Program( 'prog/tests/gf_arithmetic', ['obj/galois_field.o', 'obj/tests/test.o', 'obj/tests/gf_arithmetic.o'] )
env.Command(".test.gf_arithmetic.passed", 'prog/tests/gf_arithmetic', runTest)
env.Program( 'prog/tests/rs', ['obj/galois_field.o', 'obj/reed_solomon.o', 'obj/tests/test.o', 'obj/tests/rs.o'] )
env.Command(".test.rs.passed", 'prog/tests/rs', runTest)

