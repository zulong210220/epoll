package gopoll

import (
	"encoding/hex"
	"errors"
	"net"
	"reflect"
	"syscall"
	"unsafe"

	"github.com/zulong210220/epoll/gopoll/log"
	"golang.org/x/sys/unix"
)

const (
	EpollEventSize = 1024
)

type epoll struct {
	fd      int
	eventFD int
	efdbuf  []byte

	fdconns map[int]*Conn
	count   int32
	idx     int
}

func MkEpoll() (*epoll, error) {
	fd, err := unix.EpollCreate1(0)
	if err != nil {
		return nil, err
	}

	r0, _, errno := unix.Syscall(unix.SYS_EVENTFD2, 0, 0, 0)
	if errno != 0 {
		log.Errorf("Syscall(SYS_EVENTFD2): %v", err)
		return nil, errno
	}

	eventFD := int(r0)

	if err = unix.EpollCtl(fd, unix.EPOLL_CTL_ADD, eventFD, &unix.EpollEvent{
		Events: unix.EPOLLIN,
		Fd:     int32(eventFD),
	}); err != nil {
		unix.Close(fd)
		unix.Close(eventFD)
		return nil, err
	}
	//fmt.Printf("fd:%d eventFD:%d\n", fd, eventFD)

	return &epoll{
		fd:      fd,
		eventFD: eventFD,
		fdconns: make(map[int]*Conn),
		efdbuf:  make([]byte, 8),
	}, nil
}

func (e *epoll) Add(conn *Conn) error {
	// Extract file descriptor associated with the connection
	fd := conn.fd
	log.Debugf("epoll add fd:%d", fd)
	err := unix.EpollCtl(e.fd, syscall.EPOLL_CTL_ADD, fd, &unix.EpollEvent{Events: unix.POLLIN | unix.POLLHUP, Fd: int32(fd)})
	if err != nil {
		return err
	}

	e.fdconns[fd] = conn
	if len(e.fdconns)%100 == 0 {
		log.Infof("total number of connections: %v", len(e.fdconns))
	}
	return nil
}

func (e *epoll) Remove(conn net.Conn) error {
	fd := socketFD(conn)

	log.Debugf("epoll remove fd:%d", fd)
	if fd < 0 {
		return nil
	}
	err := unix.EpollCtl(e.fd, syscall.EPOLL_CTL_DEL, fd, nil)
	if err != nil {
		return err
	}

	delete(e.fdconns, fd)
	if len(e.fdconns)%100 == 0 {
		log.Infof("total number of connections: %v", len(e.fdconns))
	}
	return nil
}

func (e *epoll) Wait() ([]int, error) {
	events := make([]unix.EpollEvent, EpollEventSize)
	n, err := unix.EpollWait(e.fd, events, -1)
	if err != nil {
		return nil, err
	}

	var fds []int
	for i := 0; i < n; i++ {
		fd := int(events[i].Fd)
		if fd == e.eventFD {
			n, errr := syscall.Read(e.eventFD, e.efdbuf) // simply consume
			log.Infof("eventFD read n:%d err:%v buf:%s", n, errr, hex.EncodeToString(e.efdbuf))
			continue
		}
		fds = append(fds, fd)
	}
	return fds, nil
}

func (e *epoll) Close() {
	if err := unix.Close(e.fd); err != nil {
		log.Errorf("poll: epoll error: %v\n", err)
	}

	e.wakeup()
	return
}

var (
	ErrPollerClosed = errors.New("error poller closed")
)

// wakeup interrupt epoll_wait
func (e *epoll) wakeup() error {
	if e.eventFD != -1 {
		var x uint64 = 1
		// eventfd has set with EFD_NONBLOCK
		_, err := syscall.Write(e.eventFD, (*(*[8]byte)(unsafe.Pointer(&x)))[:])
		return err
	}
	return ErrPollerClosed
}

func socketFD(conn net.Conn) int {
	//tls := reflect.TypeOf(conn.UnderlyingConn()) == reflect.TypeOf(&tls.Conn{})
	// Extract the file descriptor associated with the connection
	//connVal := reflect.Indirect(reflect.ValueOf(conn)).FieldByName("conn").Elem()
	tcpConn := reflect.Indirect(reflect.ValueOf(conn)).FieldByName("conn")
	//if tls {
	//	tcpConn = reflect.Indirect(tcpConn.Elem())
	//}
	fdVal := tcpConn.FieldByName("fd")
	pfdVal := reflect.Indirect(fdVal).FieldByName("pfd")
	return int(pfdVal.FieldByName("Sysfd").Int())
}

func (e *epoll) AddReadWrite(fd int) {
	if err := syscall.EpollCtl(e.fd, syscall.EPOLL_CTL_ADD, fd,
		&syscall.EpollEvent{Fd: int32(fd),
			Events: syscall.EPOLLIN | syscall.EPOLLOUT,
		},
	); err != nil {
		panic(err)
	}
}

func (e *epoll) ModRead(fd int) {
	if err := syscall.EpollCtl(e.fd, syscall.EPOLL_CTL_MOD, fd,
		&syscall.EpollEvent{Fd: int32(fd),
			Events: syscall.EPOLLIN,
		},
	); err != nil {
		panic(err)
	}
}
