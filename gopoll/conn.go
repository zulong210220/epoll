package gopoll

import "syscall"

type Conn struct {
	fd       int
	state    int32
	sa       syscall.Sockaddr // remote socket address
	epollIdx int
	epoller  *epoll
}

const (
	StateOpen = 1
)

func (c *Conn) open() bool {
	return c.state == StateOpen
}
