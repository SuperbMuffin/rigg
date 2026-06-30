# Defer
A defer statement similar to go's defer statement would be a awesome feature.

```
defer:
{
    free(someResource);
    free(someOtherResource);
}
```

```
defer free(someResource);
```

Both of those syntax's are completely valid. The defer statement is called every time the scope is exited.

e.g. when a function returns, or when a block is exited.
