env = Environment()

env.Append(CCFLAGS=['-O3', '-Wall', '-Wextra', '-pedantic', '-Werror', '-Warray-bounds'])

env.Append(LIBPATH = ['lib/'])
env.Append(LIBS = ['ncursesw'])

env.Program(target='ste', source=['src/ste.c'])
