import os
env = Environment(CCFLAGS = '-O2 -Wall -std=c99 -Isrc -g -D_GNU_SOURCE', LIBS='readline')

env.ParseConfig('pkg-config --cflags --libs openssl')
env.ParseConfig('pkg-config --cflags --libs lua')

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
env.Object( 'obj/layers/nbd.o', 'src/layers/nbd.c' )
env.Object( 'obj/layers/baseio.o', 'src/layers/baseio.c' )
env.Object( 'obj/layers/verify.o', 'src/layers/verify.c' )
#env.Object( 'obj/layers/partitioner.o', 'src/layers/partitioner.c' )
env.Object( 'obj/layers/encrypt.o', 'src/layers/encrypt.c' )
env.Object( 'obj/layers/slice.o', 'src/layers/slice.c' )
env.Object( 'obj/layers/stripe.o', 'src/layers/stripe.c' )
env.Object( 'obj/layers/concat.o', 'src/layers/concat.c' )
env.Object( 'obj/bdev.o', 'src/bdev.c' )
env.Object( 'obj/crc.o', 'src/crc.c' )
env.Object( 'obj/block-cache.o', 'src/block-cache.c' )

env.Object( 'obj/lua/main.o', 'src/lua/main.c' )
env.Object( 'obj/lua/raw-bindings.o', 'src/lua/raw-bindings.c' )

env.Object( 'obj/tests/gf_arithmetic.o', 'src/tests/gf_arithmetic.c' )
env.Object( 'obj/tests/rs.o', 'src/tests/rs.c' )
env.Object( 'obj/tests/baseio.o', 'src/tests/baseio.c' )
env.Object( 'obj/tests/verify.o', 'src/tests/verify.c' )
env.Object( 'obj/tests/test.o', 'src/tests/test.c' )
#env.Object( 'obj/tests/partitioner.o', 'src/tests/partitioner.c' )
env.Object( 'obj/tests/encrypt.o', 'src/tests/encrypt.c' )
env.Object( 'obj/tests/slice.o', 'src/tests/slice.c' )

# final programs

env.Program( 'prog/druid', ['obj/lua/main.o', 'obj/bdev.o', 'obj/layers/baseio.o', 'obj/lua/raw-bindings.o'] )

# tests

env.Program( 'prog/tests/gf_arithmetic', ['obj/rs/galois_field.o', 'obj/tests/test.o', 'obj/tests/gf_arithmetic.o'] )
env.Command(".test.gf_arithmetic.passed", 'prog/tests/gf_arithmetic', runTest)

env.Program( 'prog/tests/rs', ['obj/rs/galois_field.o', 'obj/rs/reed_solomon.o', 'obj/tests/test.o', 'obj/tests/rs.o'] )
env.Command(".test.rs.passed", 'prog/tests/rs', runTest)

env.Program( 'prog/tests/baseio', ['obj/layers/baseio.o', 'obj/bdev.o', 'obj/tests/test.o', 'obj/tests/baseio.o'] )
env.Command(".test.baseio.passed", 'prog/tests/baseio', runTest);

env.Program( 'prog/tests/verify', ['obj/layers/verify.o', 'obj/bdev.o', 'obj/layers/baseio.o', 'obj/tests/test.o', 'obj/tests/verify.o', 'obj/crc.o'] )
env.Command(".test.verify.passed", 'prog/tests/verify', runTest);

#env.Program( 'prog/tests/partitioner', ['obj/bdev.o', 'obj/layers/baseio.o', 'obj/layers/partitioner.o', 'obj/tests/test.o', 'obj/tests/partitioner.o'] )
#env.Command(".test.partitioner.passed", 'prog/tests/partitioner', runTest);

env.Program( 'prog/tests/encrypt', ['obj/bdev.o', 'obj/layers/baseio.o', 'obj/layers/encrypt.o', 'obj/tests/test.o', 'obj/tests/encrypt.o'] )
env.Command(".test.encrypt.passed", 'prog/tests/encrypt', runTest);

env.Program( 'prog/tests/slice', ['obj/bdev.o', 'obj/layers/baseio.o', 'obj/layers/slice.o', 'obj/tests/test.o', 'obj/tests/slice.o'] )
env.Command(".test.slice.passed", 'prog/tests/slice', runTest);

