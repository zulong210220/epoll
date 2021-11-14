package gopoll

import (
	"fmt"
	"net"
	"sync"
	"sync/atomic"
	"syscall"
)

type Server struct {
	Ip       string
	Port     string
	epollers []*epoll
	ln       net.TCPListener
	fd       int
	lock     sync.Mutex
	fdconns  map[int]*Conn
	accepted int32
}

func NewServer(ip, port string) *Server {
	s := &Server{
		Ip:      ip,
		Port:    port,
		lock:    sync.Mutex{},
		fdconns: make(map[int]*Conn),
	}
	epoller, err := MkEpoll()
	if err != nil {
		panic("create epoll failed")
	}
	s.epollers = append(s.epollers, epoller)

	return s
}

func (s *Server) Close() {
	s.ln.Close()
}

func (s *Server) listen() {
	addr := fmt.Sprintf("%s:%d", s.Ip, s.Port)
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		panic(err)
	}

	tcpLn := ln.(*net.TCPListener)
	fd, err := tcpLn.File()
	if err != nil {
		//
	}

	s.fd = int(fd.Fd())
	syscall.SetNonblock(s.fd, true)
}

func (s *Server) Wait(epoller *epoll) error {
	for {
		fds, err := epoller.Wait()
		if err != nil && err != syscall.EINTR {
			return err
		}

		// handle conn
		for i := 0; i < len(fds); i++ {
			s.handleFd(epoller, fds[i])
		}
	}
}

func (s *Server) handleFd(epoller *epoll, fd int) error {
	conn := s.fdconns[fd]
	if conn == nil {
		// accept
		idx := int(atomic.LoadInt32(&s.accepted)) % len(s.epollers)
		// 只能自己accept
		if idx != epoller.idx {
			return nil // do not accept
		}
		atomic.AddInt32(&s.accepted, 1)

		return s.accept(epoller, fd, 0)
	} else if !conn.open() {
	}

	return nil
}

func (s *Server) accept(epoller *epoll, fd, i int) error {
	nfd, sa, err := syscall.Accept(fd)
	if err != nil {
		if err == syscall.EAGAIN {
			return nil
		}
		return err
	}
	if err := syscall.SetNonblock(nfd, true); err != nil {
		return err
	}
	c := &Conn{fd: nfd, sa: sa, epollIdx: i, epoller: epoller}
	epoller.fdconns[c.fd] = c
	epoller.AddReadWrite(c.fd)
	atomic.AddInt32(&epoller.count, 1)
	return err
}
