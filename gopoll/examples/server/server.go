package main

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"github.com/zulong210220/epoll/gopoll"
	"github.com/zulong210220/epoll/gopoll/log"
)

func main() {
	fmt.Println("vim-go")
	log.Init(&log.Config{
		Dir:      "../logs",
		FileSize: 256,
		FileNum:  256,
		Env:      "test",
		Level:    "INFO",
		FileName: "server",
	})
	defer log.ForceFlush()

	s := gopoll.NewServer("", "7777")
	s.Serve()

	c := make(chan os.Signal, 1)
	signal.Notify(c, syscall.SIGINT, syscall.SIGTERM)
	<-c

	s.Close()
}
