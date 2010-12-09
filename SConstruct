import os
env = Environment(CCFLAGS = '-O0 -Wall -std=c99 -Isrc -g -D_BSD_SOURCE -D_POSIX_SOURCE')

def runTest(env,target,source):
    import subprocess
    app = str(source[0].abspath)
    if not subprocess.call(app):
        open(str(target[0]),'w').write("PASSED\n")

env.Command( 'obj', [], Mkdir( '$TARGET' ) )
env.Command( 'prog', [], Mkdir( '$TARGET' ) )
env.Command( 'prog/tests', [], Mkdir( '$TARGET' ) )

# object code

env.Object( 'obj/galois_field.o', 'src/galois_field.c' )
env.Object( 'obj/reed_solomon.o', 'src/reed_solomon.c' )
env.Object( 'obj/main.o', 'src/main.c' )
env.Object( 'obj/remapper.o', 'src/remapper.c' )
env.Object( 'obj/nbd.o', 'src/nbd.c' )
env.Object( 'obj/distributor.o', 'src/distributor.c' )

env.Object( 'obj/tests/gf_arithmetic.o', 'src/tests/gf_arithmetic.c' )
env.Object( 'obj/tests/rs.o', 'src/tests/rs.c' )
env.Object( 'obj/tests/test.o', 'src/tests/test.c' )

# final programs

env.Program( 'prog/druid', ['obj/main.o', 'obj/remapper.o', 'obj/nbd.o', 'obj/distributor.o'] )

# tests

env.Program( 'prog/tests/gf_arithmetic', ['obj/galois_field.o', 'obj/tests/test.o', 'obj/tests/gf_arithmetic.o'] )
env.Command(".test.gf_arithmetic.passed", 'prog/tests/gf_arithmetic', runTest)

env.Program( 'prog/tests/rs', ['obj/galois_field.o', 'obj/reed_solomon.o', 'obj/tests/test.o', 'obj/tests/rs.o'] )
env.Command(".test.rs.passed", 'prog/tests/rs', runTest)

