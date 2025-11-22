# 1. How to add a new subsystem

- Create a directory
- Enter the directory
- Create `Kconfig` file
- Create `Makefile` file
- Add the directory to the `Kconfig` file in root: `source "my_sub_system/Kconfig"`

# 2. File System Basic Walktrough

```
    ┌──────────────┐      ┌──────────────┐      ┌──────────────┐      ┌──────────────┐
    │              │      │              │      │              │      │              │
    │ task_struct  │─────▶│ files_struct │─────▶│   fdtable    │─────▶│ fd[] array   │
    │              │files │              │ fdt  │              │ fd   │              │
    └──────────────┘      └──────────────┘      └──────────────┘      └──────────────┘
                                                                            │
                                                                            ▼
                                                                        ┌──────────────┐
                                                                        │              │
                                                                        │ struct file  │
                                                                        │              │
                                                                        └──────────────┘
    
    ┌──────────────┐        ┌──────────────┐     
    │              │        │              │     
    │ struct file  │─────▶  │ inode        │     
    │              │f_inode │              │     
    └──────────────┘        └──────────────┘     
    
    ┌──────────────┐        ┌──────────────┐        ┌──────────────┐        ┌──────────────┐     
    │              │        │              │        │              │        │              │     
    │ struct file  │─────▶  │ path         │─────▶  │ dentry       │─────▶  │ inode        │     
    │              │f_path  │              │dentry  │              │d_inode │              │     
    └──────────────┘        └──────────────┘        └──────────────┘        └──────────────┘  
```