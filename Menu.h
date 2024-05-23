typedef struct Interval {
  uint8_t hours;
  uint8_t minutes;
};

typedef struct MenuItem {
  Interval interval;
  const char *caption;
};

void increaseInterval(Interval& interval) {
    interval.minutes += 1;
    if (interval.minutes >= 60) {
        interval.hours += 1;
        interval.minutes = 0;
    }
    // return interval;
}

void decreaseInterval(Interval& interval) {
    if (interval.minutes > 0) {
        interval.minutes -= 1;
    }
    if (interval.minutes == 0) {
      if (interval.hours > 0) {
        interval.hours -= 1;
        interval.minutes = 59;
      }
    }
    // return interval;
}
