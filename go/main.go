package main

import (
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

func timeMono(t time.Time) float64 {
	s := t.String()
	// s looks like "2018-09-03 01:15:36.7263491 +0000 UTC m=+65.036704001"
	parts := strings.Split(s, " ")
	if len(parts) < 5 {
		return 0
	}
	m := parts[4]
	val, err := strconv.ParseFloat(m[3:], 32)
	if err != nil {
		return 0
	}
	return val
}

func main() {
	now := time.Now()

	for {
		fmt.Println("")
		wakeTime := now.Add(time.Second * 5)
		oldNow := now

		for now.Before(wakeTime) {
			sleepDur := wakeTime.Sub(now)
			fmt.Printf("Sleeping for %v\n", sleepDur)

			afterChan := time.After(sleepDur)
			now = <-afterChan
		}

		// do task
		fmt.Printf("Woke at %v (should be >= %v)\n", now.String(), wakeTime.String())

		oldNowSecs := float64(oldNow.UnixNano()) / 1e9
		nowSecs := float64(now.UnixNano()) / 1e9
		wallDiff := nowSecs - oldNowSecs
		monoDiff := timeMono(now) - timeMono(oldNow)
		fmt.Printf("Wall diff: %v        Mono diff: %v\n", wallDiff, monoDiff)

		if now.UnixNano() < wakeTime.UnixNano() {
			fmt.Printf("BUG ENCOUNTERED!!!\n")
			os.Exit(1)
		}
	}
}
