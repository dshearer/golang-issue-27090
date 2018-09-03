#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>

#define NSEC_PER_SEC 1000000000

/*
const (
  secondsPerMinute = 60
  secondsPerHour   = 60 * 60
  secondsPerDay    = 24 * secondsPerHour
  secondsPerWeek   = 7 * secondsPerDay
  daysPer400Years  = 365*400 + 97
  daysPer100Years  = 365*100 + 24
  daysPer4Years    = 365*4 + 1
)
*/
#define secondsPerHour (60L * 60L)
#define secondsPerDay  (24 * secondsPerHour)

/*
  unixToInternal int64 = (1969*365 + 1969/4 - 1969/100 + 1969/400) * secondsPerDay
  internalToUnix int64 = -unixToInternal

  wallToInternal int64 = (1884*365 + 1884/4 - 1884/100 + 1884/400) * secondsPerDay
  internalToWall int64 = -wallToInternal
*/
#define unixToInternal ((1969*365 + 1969/4 - 1969/100 + 1969/400) * secondsPerDay)
#define internalToUnix (-unixToInternal)
#define wallToInternal ((1884*365 + 1884/4 - 1884/100 + 1884/400) * secondsPerDay)

/*

const (
  hasMonotonic = 1 << 63
  maxWall      = wallToInternal + (1<<33 - 1) // year 2157
  minWall      = wallToInternal               // year 1885
  nsecMask     = 1<<30 - 1
  nsecShift    = 30
)
*/
#define hasMonotonic (1L << 63)
#define minWall      wallToInternal
#define nsecMask     ((1L << 30) - 1)
#define nsecShift    30

#define AND_NOT(a, b) ((a) & ~(b))

typedef struct {
  uint64_t wall;
  int64_t ext;
} go_time_t;

typedef int64_t duration_t;

/*
  minDuration Duration = -1 << 63
  maxDuration Duration = 1<<63 - 1
 */

#define minDuration ((duration_t) -0x8000000000000000)
#define maxDuration ((duration_t) 0x7fffffffffffffff)

/*
  Nanosecond  Duration = 1
  Microsecond          = 1000 * Nanosecond
  Millisecond          = 1000 * Microsecond
  Second               = 1000 * Millisecond
*/
#define Nanosecond  ((duration_t) 1)
#define Microsecond ((duration_t) (1000 * Nanosecond))
#define Millisecond ((duration_t) (1000 * Microsecond))
#define Second      ((duration_t) (1000 * Millisecond))

void _now(int64_t *sec, int32_t *nsec, int64_t *mono) {
  // get wall time
  struct timespec ts = {0};
  clock_gettime(CLOCK_REALTIME, &ts);
  *sec = ts.tv_sec;
  *nsec = ts.tv_nsec;

  // get mono time
  static int64_t first_mono = 0;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  int64_t tmp = ts.tv_sec;
  tmp = tmp * NSEC_PER_SEC + ts.tv_nsec;
  if (first_mono == 0) {
    first_mono = tmp;
  }
  *mono = tmp - first_mono;
}

/*
func Now() Time {
  sec, nsec, mono := now()
  sec += unixToInternal - minWall
  if uint64(sec)>>33 != 0 {
    return Time{uint64(nsec), sec + minWall, Local}
  }
  return Time{hasMonotonic | uint64(sec)<<nsecShift | uint64(nsec), mono, Local}
}
*/
go_time_t now() {
  int64_t sec = 0;
  int32_t nsec = 0;
  int64_t mono = 0;
  _now(&sec, &nsec, &mono);
  sec += unixToInternal - minWall;
  if (((uint64_t)sec)>>33 != 0) {
    go_time_t t = {((uint64_t)nsec), sec + minWall};
    return t;
  }
  go_time_t t = {hasMonotonic | ((uint64_t)sec)<<nsecShift | ((uint64_t)nsec), mono};
  return t;
}

/*
func (t *Time) nsec() int32 {
  return int32(t.wall & nsecMask)
}
*/
int32_t time_nsec(const go_time_t t) {
  return (int32_t) (t.wall & nsecMask);
}

/*
func (t *Time) sec() int64 {
  if t.wall&hasMonotonic != 0 {
    return wallToInternal + int64(t.wall<<1>>(nsecShift+1))
  }
  return int64(t.ext)
}
*/
int64_t time_sec(const go_time_t t) {
  if ((t.wall & hasMonotonic) != 0) {
    return wallToInternal + (int64_t)((t.wall << 1) >> (nsecShift+1));
  }
  return (int64_t) t.ext;
}

/*
func (t *Time) unixSec() int64 { return t.sec() + internalToUnix }
*/
int64_t time_unixSec(const go_time_t t) {
  return time_sec(t) + internalToUnix;
}

/*
func (t Time) UnixNano() int64 {
  return (t.unixSec())*1e9 + int64(t.nsec())
}
*/
int64_t time_unixNano(const go_time_t t) {
  return time_unixSec(t)*NSEC_PER_SEC + ((int64_t) time_nsec(t));
}

/*
func (t Time) Before(u Time) bool {
  if t.wall&u.wall&hasMonotonic != 0 {
    return t.ext < u.ext
  }
  return t.sec() < u.sec() || t.sec() == u.sec() && t.nsec() < u.nsec()
}
*/
bool time_before(const go_time_t t, const go_time_t u) {
  if ((t.wall & u.wall & hasMonotonic) != 0) {
    return t.ext < u.ext;
  }
  return time_sec(t) < time_sec(u) || (time_sec(t) == time_sec(u) &&
    time_nsec(t) < time_nsec(u));
}

/*
func (t Time) Equal(u Time) bool {
  if t.wall&u.wall&hasMonotonic != 0 {
    return t.ext == u.ext
  }
  return t.sec() == u.sec() && t.nsec() == u.nsec()
}
*/
bool time_equal(const go_time_t t, const go_time_t u) {
  if ((t.wall & u.wall & hasMonotonic) != 0) {
    return t.ext == u.ext;
  }
  return time_sec(t) == time_sec(u) && time_nsec(t) == time_nsec(u);
}

/*
func (t *Time) stripMono() {
  if t.wall&hasMonotonic != 0 {
    t.ext = t.sec()
    t.wall &= nsecMask
  }
}
*/
void time_stripMono(go_time_t *t) {
  if ((t->wall & hasMonotonic) != 0) {
    t->ext = time_sec(*t);
    t->wall &= nsecMask;
  }
}

/*
// addSec adds d seconds to the time.
func (t *Time) addSec(d int64) {
  if t.wall&hasMonotonic != 0 {
    sec := int64(t.wall << 1 >> (nsecShift + 1))
    dsec := sec + d
    if 0 <= dsec && dsec <= 1<<33-1 {
      t.wall = t.wall&nsecMask | uint64(dsec)<<nsecShift | hasMonotonic
      return
    }
    // Wall second now out of range for packed field.
    // Move to ext.
    t.stripMono()
  }

  // TODO: Check for overflow.
  t.ext += d
}
*/
void time_addSec(go_time_t *t, int64_t d) {
  if ((t->wall & hasMonotonic) != 0) {
    const int64_t sec = (int64_t)((t->wall << 1) >> (nsecShift + 1));
    const int64_t dsec = sec + d;
    if (0 <= dsec && dsec <= (1L<<33)-1) {
      t->wall = (t->wall&nsecMask) | (((uint64_t)dsec)<<nsecShift) | hasMonotonic;
      return;
    }
    // Wall second now out of range for packed field.
    // Move to ext.
    time_stripMono(t);
  }

  // TODO: Check for overflow.
  t->ext += d;
}

/*
func (t Time) Add(d Duration) Time {
  dsec := int64(d / 1e9)
  nsec := t.nsec() + int32(d%1e9)
  if nsec >= 1e9 {
    dsec++
    nsec -= 1e9
  } else if nsec < 0 {
    dsec--
    nsec += 1e9
  }
  t.wall = t.wall&^nsecMask | uint64(nsec) // update nsec
  t.addSec(dsec)
  if t.wall&hasMonotonic != 0 {
    te := t.ext + int64(d)
    if d < 0 && te > int64(t.ext) || d > 0 && te < int64(t.ext) {
      // Monotonic clock reading now out of range; degrade to wall-only.
      t.stripMono()
    } else {
      t.ext = te
    }
  }
  return t
}
*/
go_time_t time_add(go_time_t t, duration_t d) {
  int64_t dsec = (int64_t)(d / NSEC_PER_SEC);
  int32_t nsec = time_nsec(t) + (int32_t)(d%NSEC_PER_SEC);
  if (nsec >= NSEC_PER_SEC) {
    dsec++;
    nsec -= NSEC_PER_SEC;
  } else if (nsec < 0) {
    dsec--;
    nsec += NSEC_PER_SEC;
  }
  t.wall = AND_NOT(t.wall, nsecMask) | (uint64_t) nsec; // update nsec
  time_addSec(&t, dsec);
  if ((t.wall & hasMonotonic) != 0) {
    const int64_t te = t.ext + (int64_t) d;
    if ((d < 0 && te > ((int64_t) t.ext)) || (d > 0 && te < ((int64_t) t.ext))) {
      // Monotonic clock reading now out of range; degrade to wall-only.
      time_stripMono(&t);
    } else {
      t.ext = te;
    }
  }
  return t;
}

/*
func (t Time) Sub(u Time) Duration {
  if t.wall&u.wall&hasMonotonic != 0 {
    te := int64(t.ext)
    ue := int64(u.ext)
    d := Duration(te - ue)
    if d < 0 && te > ue {
      return maxDuration // t - u is positive out of range
    }
    if d > 0 && te < ue {
      return minDuration // t - u is negative out of range
    }
    return d
  }
  d := Duration(t.sec()-u.sec())*Second + Duration(t.nsec()-u.nsec())
  // Check for overflow or underflow.
  switch {
  case u.Add(d).Equal(t):
    return d // d is correct
  case t.Before(u):
    return minDuration // t - u is negative out of range
  default:
    return maxDuration // t - u is positive out of range
  }
}
*/
duration_t time_sub(const go_time_t t, const go_time_t u) {
  if ((t.wall & u.wall & hasMonotonic) != 0) {
    const int64_t te = (int64_t) t.ext;
    const int64_t ue = (int64_t) u.ext;
    const duration_t d = (duration_t) (te - ue);
    if (d < 0 && te > ue) {
      return maxDuration; // t - u is positive out of range
    }
    if (d > 0 && te < ue) {
      return minDuration; // t - u is negative out of range
    }
    return d;
  }
  const duration_t d = ((duration_t) (time_sec(t)-time_sec(u))) *Second +
    ((duration_t) (time_nsec(t)-time_nsec(u)));
  // Check for overflow or underflow.
  if (time_equal(time_add(u, d), t)) {
    return d; // d is correct
  } else if (time_before(t, u)) {
    return minDuration; // t - u is negative out of range
  } else {
    return maxDuration; // t - u is positive out of range
  }
}

/*
func (t *Time) mono() int64 {
  if t.wall&hasMonotonic == 0 {
    return 0
  }
  return t.ext
}
*/
int64_t time_mono(const go_time_t t) {
  if ((t.wall & hasMonotonic) == 0) {
    return 0;
  }
  return t.ext;
}

void print_time(const go_time_t t) {
  const time_t u = time_unixSec(t);
  const struct tm *gm = gmtime(&u);


  // print wall
  char buff[128] = {0};
  // printf("(%li) ", u);
  strftime(buff, sizeof(buff) - 1, "%H:%M:", gm);
  const double sec = gm->tm_sec + time_nsec(t) / (double) NSEC_PER_SEC;
  printf("%s%f", buff, sec);

  // print mono
  if ((t.wall & hasMonotonic) != 0) {
    const double mono = time_mono(t) / (double) NSEC_PER_SEC;
    printf(" m=%f", mono);
  }

  fflush(stdout);
}

int main() {
  while (1) {
    go_time_t n = now();
    const go_time_t wakeTime = time_add(n, 5 * Second);
    const go_time_t oldNow = n;

    while (time_before(n, wakeTime)) {
      const duration_t sleepDur = time_sub(wakeTime, n);
      const int sleepSecs = sleepDur / Second;
      printf("Sleeping for %i sec\n", sleepSecs);

      sleep(sleepSecs);
      n = now();
    }

    // do task
    printf("Woke at ");
    print_time(n);
    printf(" (should be >= ");
    print_time(wakeTime);
    printf(")\n");

    const double oldNowSecs = time_unixNano(oldNow) / (double) 1e9;
    const double nowSecs = time_unixNano(n) / (double) 1e9;
    const double wallDiff = nowSecs - oldNowSecs;
    const double monoDiff = (time_mono(n) - time_mono(oldNow)) / 1e9;
    printf("Wall diff: %.6f        Mono diff: %.6f\n", wallDiff, monoDiff);

    if (time_unixNano(n) < time_unixNano(wakeTime)) {
      printf("BUG ENCOUNTERED!!!\n");
      exit(1);
    }

    printf("\n");
  }

  return 0;
}
