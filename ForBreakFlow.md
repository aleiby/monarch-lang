# Introduction #

Here's the before and after block diagrams of a simple for loop with a break statement.  I like how you can follow the loop visually.  Notice how the optimizer collapses blocks down nicely.  We insert an "unreachable" block after break statements to avoid having multiple branches (i.e. one for the break, followed by the standard branch to the endif).

```
for (x = 0; x < 10; ++x)
{
	print x;
	
	if (x == 5)
	{
		break;
	}
}
```

| **Before:** | **After:** |
|:------------|:-----------|
| ![http://monarch-lang.googlecode.com/svn/trunk/examples/for_break_before.png](http://monarch-lang.googlecode.com/svn/trunk/examples/for_break_before.png) | ![http://monarch-lang.googlecode.com/svn/trunk/examples/for_break_after.png](http://monarch-lang.googlecode.com/svn/trunk/examples/for_break_after.png) |