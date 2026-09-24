# LICM

LICM only moves pure, safe-to-speculate scalar unary and add/sub/mul/compare
instructions. All operands must be constants, parameters, outside-loop values,
or values already hoisted from the same loop. Calls, memory operations,
division, and modulo are not hoisted. This is why `--why-not-optimized` can
truthfully report `UNSAFE_TO_SPECULATE` or `LOOP_VARIANT_OPERAND`.
