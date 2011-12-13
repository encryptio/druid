import os
env = Environment( ENV = os.environ)
env.Append(CCFLAGS = '-O2 -Wall -std=c99 -g -D_GNU_SOURCE')
env.Append(CPPPATH = 'src')
env.Append(LIBS='readline')

env.ParseConfig('pkg-config --cflags --libs openssl')
env.ParseConfig('pkg-config --cflags --libs lua')

uname = os.popen("uname").read().rstrip()

if uname == 'OpenBSD':
    # openbsd doesn't have libevent, only libev and its emulation layer.
    # unfortunately it doesn't have a .pc in the package.
    env.Append(LIBS='event')

    # OpenBSD also requires curses for its readline implementation.
    # again, no .pc for readline.
    env.Append(LIBS='curses')
else:
    env.ParseConfig('pkg-config --cflags --libs libevent')

env.Append( BUILDERS={'File2H' : Builder(action = "perl file2h.pl $SOURCE > $TARGET")} )

def runTest(env,target,source):
    import subprocess

    if not subprocess.call(map(lambda x: x.abspath, source)):
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
env.Object( 'obj/layers/lazyzero.o', 'src/layers/lazyzero.c' )
env.Object( 'obj/bdev.o', 'src/bdev.c' )
env.Object( 'obj/crc.o', 'src/crc.c' )
env.Object( 'obj/block-cache.o', 'src/block-cache.c' )
env.Object( 'obj/logger.o', 'src/logger.c' )
env.Object( 'obj/loop.o', 'src/loop.c' )

env.Object( 'obj/lua/main.o', 'src/lua/main.c' )
env.Object( 'obj/lua/bind.o', 'src/lua/bind.c' )
env.Object( 'obj/lua/bind-bdevs.o', 'src/lua/bind-bdevs.c' )
env.Object( 'obj/lua/bind-socket.o', 'src/lua/bind-socket.c' )
env.Object( 'obj/lua/bind-timer.o', 'src/lua/bind-timer.c' )
env.Object( 'obj/lua/bind-logger.o', 'src/lua/bind-logger.c' )

env.File2H( 'src/lua/AUTOGEN-porcelain-data.h', 'src/lua/druid.lua' )
Depends(env.Object( 'obj/lua/bind-porcelain.o', 'src/lua/bind-porcelain.c' ), 'src/lua/AUTOGEN-porcelain-data.h')

env.Object( 'obj/tests/gf_arithmetic.o', 'src/tests/gf_arithmetic.c' )
env.Object( 'obj/tests/rs.o', 'src/tests/rs.c' )
env.Object( 'obj/tests/baseio.o', 'src/tests/baseio.c' )
env.Object( 'obj/tests/verify.o', 'src/tests/verify.c' )
env.Object( 'obj/tests/test.o', 'src/tests/test.c' )
#env.Object( 'obj/tests/partitioner.o', 'src/tests/partitioner.c' )
env.Object( 'obj/tests/encrypt.o', 'src/tests/encrypt.c' )
env.Object( 'obj/tests/slice.o', 'src/tests/slice.c' )

# final programs

env.Program( 'prog/druid',
    ['obj/lua/main.o',
     'obj/lua/bind.o',
     'obj/lua/bind-bdevs.o',
     'obj/lua/bind-timer.o',
     'obj/lua/bind-socket.o',
     'obj/lua/bind-logger.o',
     'obj/lua/bind-porcelain.o',

     'obj/bdev.o',
     'obj/crc.o',
     'obj/block-cache.o',
     'obj/logger.o',
     'obj/loop.o',

     'obj/layers/baseio.o',
     'obj/layers/concat.o',
     'obj/layers/encrypt.o',
     'obj/layers/nbd.o',
     'obj/layers/slice.o',
     'obj/layers/stripe.o',
     'obj/layers/verify.o',
     'obj/layers/lazyzero.o'] )

# tests

env.Program( 'prog/tests/gf_arithmetic', ['obj/rs/galois_field.o', 'obj/tests/test.o', 'obj/tests/gf_arithmetic.o', 'obj/logger.o'] )
env.Command(".test.gf_arithmetic.passed", 'prog/tests/gf_arithmetic', runTest)

env.Program( 'prog/tests/rs', ['obj/rs/galois_field.o', 'obj/rs/reed_solomon.o', 'obj/tests/test.o', 'obj/tests/rs.o', 'obj/logger.o'] )
env.Command(".test.rs.passed", 'prog/tests/rs', runTest)

env.Program( 'prog/tests/baseio', ['obj/layers/baseio.o', 'obj/bdev.o', 'obj/tests/test.o', 'obj/tests/baseio.o', 'obj/logger.o'] )
env.Command(".test.baseio.passed", 'prog/tests/baseio', runTest);

env.Program( 'prog/tests/verify', ['obj/layers/verify.o', 'obj/bdev.o', 'obj/layers/baseio.o', 'obj/tests/test.o', 'obj/tests/verify.o', 'obj/crc.o', 'obj/logger.o'] )
env.Command(".test.verify.passed", 'prog/tests/verify', runTest);

#env.Program( 'prog/tests/partitioner', ['obj/bdev.o', 'obj/layers/baseio.o', 'obj/layers/partitioner.o', 'obj/tests/test.o', 'obj/tests/partitioner.o', 'obj/logger.o'] )
#env.Command(".test.partitioner.passed", 'prog/tests/partitioner', runTest);

env.Program( 'prog/tests/encrypt', ['obj/bdev.o', 'obj/layers/baseio.o', 'obj/layers/encrypt.o', 'obj/tests/test.o', 'obj/tests/encrypt.o', 'obj/logger.o'] )
env.Command(".test.encrypt.passed", 'prog/tests/encrypt', runTest);

env.Program( 'prog/tests/slice', ['obj/bdev.o', 'obj/layers/baseio.o', 'obj/layers/slice.o', 'obj/tests/test.o', 'obj/tests/slice.o', 'obj/logger.o'] )
env.Command(".test.slice.passed", 'prog/tests/slice', runTest);

# lua tests

def luaTest(script):
    env.Command(".test.lua."+script+".passed", ['prog/druid', 'tests/'+script+'.lua'], runTest)

luaTest("test-testlib")
luaTest("bdev-interface")
luaTest("sliceconcat")
luaTest("verify")
luaTest("stripe")
luaTest("lazyzero")
luaTest("tcp-client-errors")

# "expect" tests

def runExpectTest(env,target,source):
    import subprocess

    cmd = ['perl', 'expect-test.pl']
    for x in source:
        cmd.append(x.abspath)

    if not subprocess.call(cmd):
        open(str(target[0]),'w').write("PASSED\n")
        return None
    else:
        return 1

def expectTest(dirname):
    c = env.Command(".test.expect."+dirname+".passed", 'tests/'+dirname, runExpectTest)
    Depends(c, 'tests/'+dirname+'/go.lua')
    Depends(c, 'tests/'+dirname+'/expect')
    Depends(c, 'expect-test.pl')
    Depends(c, 'prog/druid')

expectTest("timer")
expectTest("stoploop")

