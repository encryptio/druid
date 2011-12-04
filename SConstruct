import os
env = Environment(CCFLAGS = '-O2 -Wall -std=c99 -Isrc -g -D_GNU_SOURCE')

def runTest(env,target,source):
    import subprocess
    app = str(source[0].abspath)
    if not subprocess.call(app):
        open(str(target[0]),'w').write("PASSED\n")
    else:
        return 1
    return None

env.Command( 'obj', [], Mkdir( '$TARGET' ) )
env.Command( 'prog', [], Mkdir( '$TARGET' ) )
env.Command( 'prog/tests', [], Mkdir( '$TARGET' ) )

# object code

env.Object( 'obj/rs/galois_field.o', 'src/rs/galois_field.c' )
env.Object( 'obj/rs/reed_solomon.o', 'src/rs/reed_solomon.c' )
env.Object( 'obj/main.o', 'src/main.c' )
env.Object( 'obj/nbd.o', 'src/nbd.c' )
env.Object( 'obj/baseio.o', 'src/baseio.c' )
env.Object( 'obj/verify.o', 'src/verify.c' )
env.Object( 'obj/partitioner.o', 'src/partitioner.c' )
env.Object( 'obj/bdev.o', 'src/bdev.c' )
env.Object( 'obj/crc.o', 'src/crc.c' )

env.Object( 'obj/tests/gf_arithmetic.o', 'src/tests/gf_arithmetic.c' )
env.Object( 'obj/tests/rs.o', 'src/tests/rs.c' )
env.Object( 'obj/tests/baseio.o', 'src/tests/baseio.c' )
env.Object( 'obj/tests/verify.o', 'src/tests/verify.c' )
env.Object( 'obj/tests/test.o', 'src/tests/test.c' )

# final programs

env.Program( 'prog/druid', ['obj/main.o', 'obj/nbd.o', 'obj/bdev.o', 'obj/baseio.o', 'obj/verify.o', 'obj/partitioner.o', 'obj/crc.o'] )

# tests

env.Program( 'prog/tests/gf_arithmetic', ['obj/rs/galois_field.o', 'obj/tests/test.o', 'obj/tests/gf_arithmetic.o'] )
env.Command(".test.gf_arithmetic.passed", 'prog/tests/gf_arithmetic', runTest)

env.Program( 'prog/tests/rs', ['obj/rs/galois_field.o', 'obj/rs/reed_solomon.o', 'obj/tests/test.o', 'obj/tests/rs.o'] )
env.Command(".test.rs.passed", 'prog/tests/rs', runTest)

env.Program( 'prog/tests/baseio', ['obj/baseio.o', 'obj/bdev.o', 'obj/tests/test.o', 'obj/tests/baseio.o'] )
env.Command(".test.baseio.passed", 'prog/tests/baseio', runTest);

env.Program( 'prog/tests/verify', ['obj/verify.o', 'obj/bdev.o', 'obj/baseio.o', 'obj/tests/test.o', 'obj/tests/verify.o', 'obj/crc.o'] )
env.Command(".test.verify.passed", 'prog/tests/verify', runTest);

