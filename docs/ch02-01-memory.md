## Automatic Memory Management

*** WORK IN PROGRESS (subject to change) ***
### Memory Ownership Semantics

For simplicity, heap allocated objects are referred to as "persistent objects",
while stack allocated and borrowed objects are referred to as "local objects".

#### Ownership rules:

- All constructed objects live on the stack unless they are explicitly heap allocated with `new`.

- Objects passed to a function are "borrowed" by that function (whether originally local or persistent).

- All local objects can be aliased by locally constructed stack objects.

- Heap allocated objects cannot be constructed from local objects.

- Heap allocated objects cannot be aliased - ownership is instead transferred.

- Only heap allocated objects (or of course primitives e.g. int, float) can be returned from a function.

- Objects returned by a function call are guaranteed to be heap allocated,
  and so have the same semantics as objects allocated with `new`.

- Heap allocated objects that go out of scope are automatically freed.

These rules ensure that the lifetime of objects are statically known,
as aliasing is only allowed when it doesn't effect the lifetime of an object,
thus allowing memory to be automatically freed.

Example showing all of the rules in action:

``` ruby
def foo(arg)
  # stack allocated local:
  a = Some([5,26])

  # OK: local object can be built from borrowed object
  # note: this aliases "arg", so "arg" is still a valid identifier after this
  b = Some(arg)

  # OK: aliasing local object
  c = b

  # note that both are still accessible:
  print(b)
  print(c)

  # heap allocated object:
  # note: `new` applies to the constructor's args as well,
  #       so "[1957]" has the semantics of "new [1957]"
  d = new Some([1957])

  # ERROR: cannot construct heap object by aliasing borrowed or stack allocated
  #        objects
  # e = new FooBar(a, arg)

  # OK: construct heap object by taking ownership of other heap objects
  e1 = new [1,2,3,4]
  e2 = new Some(e1)

  # ERROR: can't construct a local directly from a persistent object
  #        as it would be part local, part persistent
  # e3 = Some(e2)

  # ERROR: ownership of e1's object was transferred to e2,
  #        so e1 is no longer accessible
  # print(e1)

  # this has same semantics as `new Object()`
  f = some_function()

  # ERROR: stack allocated objects cannot be returned from function
  # return a

  # OK: only heap allocated objects can be returned.
  # Since e2 and f go out of scope here, they are freed
  return d
end
```

Pattern matching has almost the same semantics as a function call,
except we differentiate two scenarios (to make life easier for you).

If pattern is heap allocated:
- pattern is borrowed
- any objects returned as match expression result must be heap allocated
  (to ensure heap allocated pattern is not aliased)
Else if pattern is a local object:
- can return another local stack allocated object

``` ruby
def bar(arg1, arg2)
  # OK: Pair is local, constructed from local args
  x = match Pair(arg1, arg2)
    # also OK: we can alias args as they are local
    Pair(King, second) => second
    Pair(first, _) => first
  end

  y = new Some("Hello!")
  # y is borrowed here
  match y
    # ERROR: cannot alias persistent object's fields in expression result
    # Some(a) => a
    # OK: print just returns "()", which is a primitive
    Some(a) => print(a)
    None => print("Impossible!")
  end
end
```

In a nutshell, safe ownership is ensured by only ever demoting an object's status from persistent to local,
and never the other way around. Thus we can never have an alias to a persistent object in the same scope
as the original object, ensuring we don't double free (or leak) the memory.


TODO: Solve the problem of relations between objects, e.g. a graph.

One approach is to add a reference counted shared pointer type.

Another approach is to create a closure, which allows all internal objects
to be local, thus allowing safe aliasing.
For example, using an actor type:

``` ruby
actor graph()
  nodes = {}
  edges = []

  def add_node(name, node)
    nodes[name] = node
  end

  def add_edge(n1, n2)
    edges.append(Edge(nodes[n1], nodes[n2]))
  end
end
```

For this to work, need to ensure that actor instance and any objects
passed to it obey the same rules as above object construction rules:
- both local
OR
- both persistent

For example:
``` ruby
def main()
  g = graph()
  node = Node(...)
  # OK: both g and node are locals
  g->add_node("myNode", node)
  node2 = new Node(...)
  # ERROR: this would make local g have some persistent state
  g->add_node("myNode", node2)

  g2 = new graph()
  # OK: ownership of node2 is transferred to g2
  g2->add_node("myPersistentNode", node2)
```
