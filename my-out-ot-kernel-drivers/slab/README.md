# Slab Cache Example Driver

This directory contains a minimal Linux kernel character device module demonstrating a slab cache.

## What it shows

- `kmem_cache_create()` to create a named slab cache
- `kmem_cache_alloc()` to allocate same-sized objects
- `kmem_cache_free()` to return objects to the cache
- `kmem_cache_destroy()` when the module unloads
- ioctl operations to create, lookup, and destroy cached objects

## Build

```bash
make all
```

## Load the module

```bash
make load
```

## Example shell command to test

```bash
make test
```

## Graph

```
              +-----------------+
              |  example_drv    |
              |  module loaded  |
              |  my_cache alloc |<-------------------+
              +-----------------+                    |
                        |                            |
                        v                            |
               +---------------------+               |
               |  slab cache         |               |
               |  (my_cache)         |               |
               +---------------------+               |
                        |                            |
       kmem_cache_alloc |                            |
                        v                            |
                +-------------------+                |
                |   cache_node      |                |
                | +---------------+ |                |
                | | struct my_data| |                |
                | |  id, value    | |                |
                | +---------------+ |                |
                +-------------------+                |
                        |                            |
                        v                            |
                +-------------------+                |
                |  kernel list      |<---------------+
                |  node_list head   |
                +-------------------+
                        |
        +---------------+---------------+
        |               |               |
  create ioctl    get ioctl     destroy ioctl
  IOCTL_CREATE    IOCTL_GET     IOCTL_DESTROY
   alloc node     lookup node   free all nodes
```

## Unload

```bash
make unload
```

## Notes
If `/dev/example_drv` does not appear automatically, check `sudo dmesg -w` and make sure your system has udev enabled.
