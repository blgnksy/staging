import fcntl
import struct
import os

TEST_DRIVER_MAGIC = ord('t')

IOC_NRBITS = 8
IOC_TYPEBITS = 8
IOC_SIZEBITS = 14
IOC_DIRBITS = 2
IOC_NRSHIFT = 0
IOC_TYPESHIFT = IOC_NRSHIFT + IOC_NRBITS
IOC_SIZESHIFT = IOC_TYPESHIFT + IOC_TYPEBITS
IOC_DIRSHIFT = IOC_SIZESHIFT + IOC_SIZEBITS
IOC_NONE = 0
IOC_WRITE = 1
IOC_READ = 2


def _IOC(direction, type_, nr, size):
    return (direction << IOC_DIRSHIFT) | (type_ << IOC_TYPESHIFT) | (nr << IOC_NRSHIFT) | (size << IOC_SIZESHIFT)


def _IO(type_, nr):
    return _IOC(IOC_NONE, type_, nr, 0)


def _IOW(type_, nr, size):
    return _IOC(IOC_WRITE, type_, nr, size)


def _IOWR(type_, nr, size):
    return _IOC(IOC_READ | IOC_WRITE, type_, nr, size)


IOCTL_CREATE = _IOW(TEST_DRIVER_MAGIC, 0, struct.calcsize('ii'))
IOCTL_GET = _IOWR(TEST_DRIVER_MAGIC, 1, struct.calcsize('ii'))
IOCTL_DESTROY = _IO(TEST_DRIVER_MAGIC, 2)


def main():
    path = '/dev/slab_drv'
    if not os.path.exists(path):
        raise FileNotFoundError(f'{path} does not exist; is the module loaded?')

    with open(path, 'rb+', buffering=0) as fd:
        create_buf = struct.pack('ii', 1, 42)
        fcntl.ioctl(fd, IOCTL_CREATE, create_buf)

        create_buf = struct.pack('ii', 2, 6)
        fcntl.ioctl(fd, IOCTL_CREATE, create_buf)

        create_buf = struct.pack('ii', 3, 26)
        fcntl.ioctl(fd, IOCTL_CREATE, create_buf)

        read_buf = struct.pack('ii', 1, 0)
        out = fcntl.ioctl(fd, IOCTL_GET, read_buf)
        print('read:', struct.unpack('ii', out))

        fcntl.ioctl(fd, IOCTL_DESTROY)


if __name__ == '__main__':
    main()
