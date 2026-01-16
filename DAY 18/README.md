# Day 18

- Implement **Pass 4: Introduce temporaries (ANF).**

Example:

```
a + b * c → let t1 = b * c; let t2 = a + t1;
```

*Source: `day0plan.md`