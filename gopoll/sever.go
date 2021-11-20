package gopoll

import (
	"fmt"
	"net"
	"sync"
	"sync/atomic"
	"syscall"

	"github.com/zulong210220/epoll/gopoll/log"
	"golang.org/x/sys/unix"
)

type Server struct {
	Ip       string
	Port     string
	epollers []*epoll
	ln       *net.TCPListener
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

	for i := 0; i < 4; i++ {
		epoller, err := MkEpoll(i + 1)
		if err != nil {
			panic("create epoll failed")
		}
		s.epollers = append(s.epollers, epoller)
	}

	return s
}

func (s *Server) Serve() {
	s.listen()
	go s.Accept()
	go s.Start()
}

func (s *Server) Close() {
	s.ln.Close()
}

func (s *Server) listen() {
	addr := fmt.Sprintf("%s:%s", s.Ip, s.Port)
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		panic(err)
	}

	tcpLn := ln.(*net.TCPListener)
	fd, err := tcpLn.File()
	if err != nil {
		//
	}
	s.ln = tcpLn

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
	fun := "Server.handleFd"
	data := make([]byte, 8)
	_, err := syscall.Read(fd, data)
	if err != nil {
		log.Errorf("%s Read epoll[%d]:%d fd:%d failed err:%v", fun, epoller.idx, epoller.fd, fd, err)
		if err == syscall.EAGAIN {
			return nil
		}
		err = unix.EpollCtl(epoller.fd, syscall.EPOLL_CTL_DEL, fd, nil)
		if err != nil {
			log.Errorf("%s Read Remove epoll[%d]:%d fd:%d failed err:%v", fun, epoller.idx, epoller.fd, fd, err)
			return err
		}
		return err
	}

	_, err = syscall.Write(fd, data)
	if err != nil {
		log.Errorf("%s Write epoll[%d]:%d fd:%d failed err:%v", fun, epoller.idx, epoller.fd, fd, err)
		err = unix.EpollCtl(epoller.fd, syscall.EPOLL_CTL_DEL, fd, nil)
		if err != nil {
			log.Errorf("%s Write Remove epoll[%d]:%d fd:%d failed err:%v", fun, epoller.idx, epoller.fd, fd, err)
			return err
		}
		return err
	}

	return err
}

func (s *Server) handleFd0(epoller *epoll, fd int) error {
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

func (s *Server) Accept() {
	if s.ln == nil {
		return
	}

	for {
		conn, err := s.ln.Accept()
		if err != nil {
			log.Errorf("Accept failed err:%v", err)
			continue
		}
		ne := len(s.epollers)
		if ne < 1 {
			continue
		}

		idx := int(s.accepted) % ne
		ep := s.epollers[idx]

		cc := &Conn{
			fd: socketFD(conn),
		}
		err = ep.Add(cc)
		if err != nil {
			log.Errorf("Accept Add failed err:%v", err)
		}

		log.Infof("Accept fd:%d ok", cc.fd)
		s.accepted++
	}
}

func (s *Server) Start() {
	for _, ep := range s.epollers {
		go s.serveEp(ep)
	}
}

func (s *Server) serveEp(ep *epoll) {
	for {
		conns, err := ep.Wait()
		if err != nil {
			log.Warningf("serveEp failed err:%v", err)
			continue
		}

		for _, conn := range conns {
			s.handleFd(ep, conn)
		}
	}
}
