enum InstGroups {
   Integer = 0, I64 = 1, Float = 2, Double = 3,
   Add = 0<<2, Mul = 1<<2, Div = 2<<2, Mod = 3<<2,
   Last = Double|Mod,
   NumGroups
};
