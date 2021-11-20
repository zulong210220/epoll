package main

import (
	"fmt"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

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
		FileName: "client",
	})
	defer log.ForceFlush()
	time.Sleep(100 * time.Millisecond)

	for i := 0; i < 10; i++ {
		go func() {
			conn, err := net.DialTimeout("tcp", ":7777", 5*time.Second)
			if err != nil {
				log.Errorf("DialTimeout failed err:%v", err)
				return
			}
			defer conn.Close()
			for i := 0; i < 100; i++ {
				data := []byte("ok")
				n, err := conn.Write(data)
				if err != nil {
					log.Errorf("conn Write %d failed err:%v", n, err)
					return
				}

			}
			time.Sleep(10 * time.Second)
		}()

	}

	c := make(chan os.Signal, 1)
	signal.Notify(c, syscall.SIGINT, syscall.SIGTERM)
	<-c

}
