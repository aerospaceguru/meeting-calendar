#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

// Constants
#define MAX_DAYS 4
#define MAX_WEEKS 4
#define MAX_SLOTS 14 // 09:00–16:30, excluding breaks
#define MAX_MEETINGS 100
#define MAX_RESERVATIONS 50
#define MAX_STR 64

const char *DAYS[MAX_DAYS] = {"Monday", "Tuesday", "Wednesday", "Thursday"};
const char *TIME_SLOTS[MAX_SLOTS] = {
    "09:00", "09:30", "10:00", "10:30", "11:00", "11:30",
    "13:00", "13:30", "14:00", "14:30", "15:00", "15:30",
    "16:00", "16:30"
};
const char *BREAK_SLOTS[2] = {"12:00", "12:30"};
const char *FREQUENCIES[4] = {"weekly", "fortnightly", "third_week", "monthly"};
const int DURATIONS[3] = {1, 2, 3}; // 30, 60, 90 min in 30-min slots

// Structures
typedef struct {
    char name[MAX_STR];
    char type[MAX_STR];
    int duration; // Slots (1=30min, 2=60min, 3=90min)
    int preferred_hours[8]; // Indices of TIME_SLOTS; -1 terminated list if not used
    char fixed_day[MAX_STR];
    char fixed_time[MAX_STR];
    char frequency[MAX_STR];
} Meeting;

typedef struct {
    char day[MAX_STR];
    char start_time[MAX_STR];
    int duration; // in slots (each slot is 30 min)
} Reservation;

typedef struct {
    int week;
    int day;       // index of day (0=Monday … 3=Thursday)
    int start_time; // index in TIME_SLOTS
    char name[MAX_STR];
    char type[MAX_STR];
    int duration;  // in slots
    char frequency[MAX_STR];
} ScheduleEntry;

typedef struct {
    ScheduleEntry schedule[MAX_MEETINGS * MAX_WEEKS];
    int schedule_count;
    Reservation reservations[MAX_RESERVATIONS];
    int reservation_count;
    double total_hours[MAX_DAYS];   // Meetings + reservations (over MAX_WEEKS)
    double meeting_hours[MAX_DAYS];   // Meetings only
    bool blocked_slots[MAX_WEEKS][MAX_DAYS][MAX_SLOTS];
} MeetingScheduler;

// Utility functions
int find_slot_index(const char *time) {
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (strcmp(TIME_SLOTS[i], time) == 0)
            return i;
    }
    return -1;
}

int find_day_index(const char *day) {
    for (int i = 0; i < MAX_DAYS; i++) {
        if (strcmp(DAYS[i], day) == 0)
            return i;
    }
    return -1;
}

bool is_break_slot(const char *time) {
    return (strcmp(time, BREAK_SLOTS[0]) == 0 || strcmp(time, BREAK_SLOTS[1]) == 0);
}

double slot_to_hour(int slot_idx) {
    int hour = slot_idx / 2 + 9;
    int minute = (slot_idx % 2) * 30;
    // Account for the break (skip the hour from 12:00–13:00)
    if (slot_idx >= 6) 
        hour++;
    return hour + minute / 60.0;
}

void compute_end_time(int start_idx, int duration_slots, char *end_time) {
    double start_hour = slot_to_hour(start_idx);
    double end_hour = start_hour + duration_slots * 0.5;
    int end_h = (int)end_hour;
    int end_m = (int)((end_hour - end_h) * 60);
    snprintf(end_time, 8, "%02d:%02d", end_h, end_m);
}

// Scheduling functions
void init_scheduler(MeetingScheduler *scheduler) {
    scheduler->schedule_count = 0;
    scheduler->reservation_count = 0;
    memset(scheduler->total_hours, 0, sizeof(scheduler->total_hours));
    memset(scheduler->meeting_hours, 0, sizeof(scheduler->meeting_hours));
    memset(scheduler->blocked_slots, 0, sizeof(scheduler->blocked_slots));
}

bool reserve_slot(MeetingScheduler *scheduler, const char *day, const char *start_time, int duration_minutes) {
    int day_idx = find_day_index(day);
    int start_idx = find_slot_index(start_time);
    if (day_idx == -1 || start_idx == -1 || duration_minutes % 30 != 0 ||
        duration_minutes < 30 || duration_minutes > 90) {
        printf("Error: Invalid reservation: %s %s %d min\n", day, start_time, duration_minutes);
        return false;
    }
    int duration_slots = duration_minutes / 30;

    // Validate the slot range
    double start_hour = slot_to_hour(start_idx);
    double end_hour = start_hour + duration_slots * 0.5;
    if (end_hour > 17.0 || is_break_slot(start_time))
        return false;
    for (int i = 0; i < duration_slots; i++) {
        int slot_idx = start_idx + i;
        if (slot_idx >= MAX_SLOTS || is_break_slot(TIME_SLOTS[slot_idx]))
            return false;
    }

    // Mark this slot as reserved for each week
    for (int week = 0; week < MAX_WEEKS; week++) {
        for (int i = 0; i < duration_slots; i++) {
            int slot_idx = start_idx + i;
            if (scheduler->blocked_slots[week][day_idx][slot_idx]) {
                printf("Error: Slot %s %s already reserved\n", day, TIME_SLOTS[slot_idx]);
                return false;
            }
            scheduler->blocked_slots[week][day_idx][slot_idx] = true;
        }
    }
    Reservation *res = &scheduler->reservations[scheduler->reservation_count++];
    strcpy(res->day, day);
    strcpy(res->start_time, start_time);
    res->duration = duration_slots;
    scheduler->total_hours[day_idx] += duration_slots * 0.5 * MAX_WEEKS;
    return true;
}

bool is_valid_slot(MeetingScheduler *scheduler, int week, int day_idx, int start_idx, int duration_slots) {
    if (start_idx < 0 || start_idx >= MAX_SLOTS || is_break_slot(TIME_SLOTS[start_idx]))
        return false;
    double start_hour = slot_to_hour(start_idx);
    double end_hour = start_hour + duration_slots * 0.5;
    if (end_hour > 17.0)
        return false;
    for (int i = 0; i < duration_slots; i++) {
        int slot_idx = start_idx + i;
        if (slot_idx >= MAX_SLOTS || is_break_slot(TIME_SLOTS[slot_idx]))
            return false;
        if (scheduler->blocked_slots[week][day_idx][slot_idx])
            return false;
    }
    return true;
}

bool add_meeting(MeetingScheduler *scheduler, Meeting *meeting) {
    int duration_slots = meeting->duration;
    int occurrences = (strcmp(meeting->frequency, "weekly") == 0) ? 4 :
                      (strcmp(meeting->frequency, "fortnightly") == 0) ? 2 : 1;
    int fixed_day_idx = meeting->fixed_day[0] ? find_day_index(meeting->fixed_day) : -1;
    int fixed_time_idx = meeting->fixed_time[0] ? find_slot_index(meeting->fixed_time) : -1;
    int assigned_weeks[MAX_WEEKS] = {0};
    int chosen_day = -1, chosen_time = -1;

    // Create an order for days based on total hours booked
    int day_order[MAX_DAYS];
    for (int i = 0; i < MAX_DAYS; i++) 
        day_order[i] = i;
    for (int i = 0; i < MAX_DAYS - 1; i++) {
        for (int j = 0; j < MAX_DAYS - i - 1; j++) {
            if (scheduler->total_hours[day_order[j]] > scheduler->total_hours[day_order[j + 1]]) {
                int temp = day_order[j];
                day_order[j] = day_order[j + 1];
                day_order[j + 1] = temp;
            }
        }
    }

    // Randomize the week order
    int weeks[MAX_WEEKS] = {0, 1, 2, 3};
    for (int i = MAX_WEEKS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = weeks[i];
        weeks[i] = weeks[j];
        weeks[j] = temp;
    }

    // Find a consistent slot
    double min_hours = 1e9;
    for (int w = 0; w < MAX_WEEKS; w++) {
        int day_start = (fixed_day_idx >= 0) ? fixed_day_idx : 0;
        int day_end = (fixed_day_idx >= 0) ? fixed_day_idx + 1 : MAX_DAYS;
        for (int d = 0; d < day_end - day_start; d++) {
            int day_idx = (fixed_day_idx >= 0) ? fixed_day_idx : day_order[d];
            if (scheduler->meeting_hours[day_idx] / 4 > 2.5)
                continue; // Cap at 2.5 hours/week
            int time_start = (fixed_time_idx >= 0) ? fixed_time_idx : 0;
            int time_end = (fixed_time_idx >= 0) ? fixed_time_idx + 1 : MAX_SLOTS;
            if (meeting->preferred_hours[0] >= 0 && fixed_time_idx < 0) {
                time_end = 0;
                while (meeting->preferred_hours[time_end] >= 0 && time_end < 8)
                    time_end++;
            }
            for (int t = time_start; t < time_end && t < MAX_SLOTS; t++) {
                int time_idx = (fixed_time_idx >= 0) ? fixed_time_idx :
                               meeting->preferred_hours[0] >= 0 ? meeting->preferred_hours[t] : t;
                if (time_idx < 0 || time_idx >= MAX_SLOTS)
                    continue;
                int valid_weeks = 0;
                double avg_hours = 0;
                for (int ww = 0; ww < MAX_WEEKS; ww++) {
                    if (assigned_weeks[ww])
                        continue;
                    if (is_valid_slot(scheduler, ww, day_idx, time_idx, duration_slots)) {
                        valid_weeks++;
                        avg_hours += scheduler->total_hours[day_idx] + duration_slots * 0.5;
                    }
                    if (valid_weeks >= occurrences)
                        break;
                }
                if (valid_weeks >= occurrences && avg_hours / valid_weeks < min_hours) {
                    min_hours = avg_hours / valid_weeks;
                    chosen_day = day_idx;
                    chosen_time = time_idx;
                }
            }
        }
    }

    if (chosen_day == -1 || chosen_time == -1) {
        printf("Error: No consistent slot for %s (%s)\n", meeting->name, meeting->frequency);
        return false;
    }

    // Assign the meeting to the chosen slots across the required weeks
    for (int occ = 0; occ < occurrences; occ++) {
        int week = -1;
        bool found = false;
        for (int w = 0; w < MAX_WEEKS; w++) {
            week = weeks[w];
            if (assigned_weeks[week])
                continue;
            if (is_valid_slot(scheduler, week, chosen_day, chosen_time, duration_slots)) {
                found = true;
                break;
            }
        }
        if (!found) {
            printf("Error: Cannot assign week %d for %s (%s)\n", occ + 1, meeting->name, meeting->frequency);
            return false;
        }
        ScheduleEntry *entry = &scheduler->schedule[scheduler->schedule_count++];
        entry->week = week;
        entry->day = chosen_day;
        entry->start_time = chosen_time;
        strcpy(entry->name, meeting->name);
        strcpy(entry->type, meeting->type);
        entry->duration = duration_slots;
        strcpy(entry->frequency, meeting->frequency);
        assigned_weeks[week] = 1;
        scheduler->total_hours[chosen_day] += duration_slots * 0.5;
        scheduler->meeting_hours[chosen_day] += duration_slots * 0.5;
        for (int i = 0; i < duration_slots; i++) {
            scheduler->blocked_slots[week][chosen_day][chosen_time + i] = true;
        }
    }
    return true;
}

void display_schedule(MeetingScheduler *scheduler) {
    printf("\nWeekly Meeting Schedule (4-week cycle):\n");
    double total_meeting_hours[MAX_DAYS] = {0};
    for (int week = 0; week < MAX_WEEKS; week++) {
        printf("\nWeek %d:\n", week + 1);
        for (int day = 0; day < MAX_DAYS; day++) {
            printf("  %s:\n", DAYS[day]);
            ScheduleEntry entries[MAX_MEETINGS];
            int entry_count = 0;
            // Add meetings
            for (int i = 0; i < scheduler->schedule_count; i++) {
                if (scheduler->schedule[i].week == week && scheduler->schedule[i].day == day) {
                    entries[entry_count++] = scheduler->schedule[i];
                }
            }
            // Add external reservations
            for (int i = 0; i < scheduler->reservation_count; i++) {
                if (strcmp(scheduler->reservations[i].day, DAYS[day]) == 0) {
                    ScheduleEntry *e = &entries[entry_count++];
                    e->week = week;
                    e->day = day;
                    e->start_time = find_slot_index(scheduler->reservations[i].start_time);
                    strcpy(e->name, "Reserved (External)");
                    strcpy(e->type, "reserved");
                    e->duration = scheduler->reservations[i].duration;
                    strcpy(e->frequency, "weekly");
                }
            }
            // Sort entries by start time
            for (int i = 0; i < entry_count - 1; i++) {
                for (int j = 0; j < entry_count - i - 1; j++) {
                    if (entries[j].start_time > entries[j + 1].start_time) {
                        ScheduleEntry temp = entries[j];
                        entries[j] = entries[j + 1];
                        entries[j + 1] = temp;
                    }
                }
            }
            if (entry_count == 0)
                printf("    No meetings.\n");
            else {
                for (int i = 0; i < entry_count; i++) {
                    char end_time[8];
                    compute_end_time(entries[i].start_time, entries[i].duration, end_time);
                    printf("    %s–%s - %s (%s, %d min, %s)\n",
                           TIME_SLOTS[entries[i].start_time], end_time,
                           entries[i].name, entries[i].type,
                           entries[i].duration * 30, entries[i].frequency);
                    if (strcmp(entries[i].type, "reserved") != 0) {
                        int occ = (strcmp(entries[i].frequency, "weekly") == 0) ? 4 :
                                  (strcmp(entries[i].frequency, "fortnightly") == 0) ? 2 : 1;
                        total_meeting_hours[day] += entries[i].duration * 0.5 * occ / 4;
                    }
                }
            }
        }
    }
    printf("\nFriday: No meetings.\n");
    printf("\nAverage meeting hours per day (over 4 weeks):");
    for (int d = 0; d < MAX_DAYS; d++) {
        printf(" %s: %.1f", DAYS[d], total_meeting_hours[d]);
    }
    printf("\nTotal hours per day (meetings + reservations):");
    for (int d = 0; d < MAX_DAYS; d++) {
        double total = scheduler->meeting_hours[d] + (scheduler->total_hours[d] - scheduler->meeting_hours[d]);
        printf(" %s: %.1f", DAYS[d], total / 4);
    }
    printf("\n");
}

void export_to_ics(MeetingScheduler *scheduler, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Cannot open %s for writing.\n", filename);
        return;
    }
    fprintf(fp, "BEGIN:VCALENDAR\n");
    fprintf(fp, "PRODID:-//Meeting Scheduler//xAI//EN\n");
    fprintf(fp, "VERSION:2.0\n");

    // Use a base date (for example, Monday, April 14, 2025)
    struct tm base_date = {0};
    base_date.tm_year = 2025 - 1900;
    base_date.tm_mon = 3;
    base_date.tm_mday = 14;
    base_date.tm_hour = 0;
    base_date.tm_min = 0;
    base_date.tm_sec = 0;
    mktime(&base_date);

    // Group schedule entries by meeting details
    typedef struct {
        char name[MAX_STR];
        char type[MAX_STR];
        int duration;
        char frequency[MAX_STR];
        int weeks[MAX_WEEKS];
        int week_count;
        int day;
        int start_time;
    } Event;
    Event events[MAX_MEETINGS];
    int event_count = 0;
    
    for (int i = 0; i < scheduler->schedule_count; i++) {
        ScheduleEntry *s = &scheduler->schedule[i];
        int j;
        for (j = 0; j < event_count; j++) {
            if (strcmp(events[j].name, s->name) == 0 &&
                strcmp(events[j].type, s->type) == 0 &&
                events[j].duration == s->duration &&
                strcmp(events[j].frequency, s->frequency) == 0) {
                events[j].weeks[events[j].week_count++] = s->week;
                break;
            }
        }
        if (j == event_count) {
            Event *e = &events[event_count++];
            strcpy(e->name, s->name);
            strcpy(e->type, s->type);
            e->duration = s->duration;
            strcpy(e->frequency, s->frequency);
            e->weeks[0] = s->week;
            e->week_count = 1;
            e->day = s->day;
            e->start_time = s->start_time;
        }
    }

    // Write events
    for (int i = 0; i < event_count; i++) {
        Event *e = &events[i];
        int min_week = e->weeks[0];
        for (int j = 1; j < e->week_count; j++) {
            if (e->weeks[j] < min_week)
                min_week = e->weeks[j];
        }
        struct tm dtstart = base_date;
        dtstart.tm_mday += e->day + min_week * 7;
        int hour = e->start_time / 2 + 9;
        if (e->start_time >= 6)
            hour++;
        int minute = (e->start_time % 2) * 30;
        dtstart.tm_hour = hour;
        dtstart.tm_min = minute;
        mktime(&dtstart);
        char dtstart_str[32];
        strftime(dtstart_str, sizeof(dtstart_str), "%Y%m%dT%H%M%S", &dtstart);
        fprintf(fp, "BEGIN:VEVENT\n");
        fprintf(fp, "SUMMARY:%s (%s)\n", e->name, e->type);
        fprintf(fp, "DTSTART:%s\n", dtstart_str);
        fprintf(fp, "DURATION:PT%dM\n", e->duration * 30);
        fprintf(fp, "RRULE:FREQ=WEEKLY;INTERVAL=%d\n",
                (strcmp(e->frequency, "weekly") == 0) ? 1 :
                (strcmp(e->frequency, "fortnightly") == 0) ? 2 :
                (strcmp(e->frequency, "third_week") == 0) ? 3 : 4);
        fprintf(fp, "DESCRIPTION:Type: %s, Duration: %d min, Frequency: %s\n",
                e->type, e->duration * 30, e->frequency);
        fprintf(fp, "END:VEVENT\n");
    }
    // Export reservations as separate events
    for (int i = 0; i < scheduler->reservation_count; i++) {
        Reservation *r = &scheduler->reservations[i];
        struct tm dtstart = base_date;
        dtstart.tm_mday += find_day_index(r->day);
        int start_idx = find_slot_index(r->start_time);
        dtstart.tm_hour = start_idx / 2 + 9;
        if (start_idx >= 6)
            dtstart.tm_hour++;
        dtstart.tm_min = (start_idx % 2) * 30;
        mktime(&dtstart);
        char dtstart_str[32];
        strftime(dtstart_str, sizeof(dtstart_str), "%Y%m%dT%H%M%S", &dtstart);
        fprintf(fp, "BEGIN:VEVENT\n");
        fprintf(fp, "SUMMARY:Reserved (External)\n");
        fprintf(fp, "DTSTART:%s\n", dtstart_str);
        fprintf(fp, "DURATION:PT%dM\n", r->duration * 30);
        fprintf(fp, "RRULE:FREQ=WEEKLY\n");
        fprintf(fp, "DESCRIPTION:External commitment, Duration: %d min\n", r->duration * 30);
        fprintf(fp, "END:VEVENT\n");
    }
    
    fprintf(fp, "END:VCALENDAR\n");
    fclose(fp);
    printf("\nSchedule exported to %s\n", filename);
}

// --------------------
// Interactive Front End
// --------------------
void interactive_menu() {
    MeetingScheduler scheduler;
    init_scheduler(&scheduler);
    char input[128];
    int choice;

    while (1) {
        printf("\n--- Meeting Scheduler Menu ---\n");
        printf("1. Add Reservation\n");
        printf("2. Add Meeting\n");
        printf("3. Display Schedule\n");
        printf("4. Export Schedule to ICS\n");
        printf("5. Load Sample Data\n");
        printf("6. Quit\n");
        printf("Enter your choice: ");
        if (fgets(input, sizeof(input), stdin) != NULL)
            choice = atoi(input);
        else
            break;

        switch (choice) {
            case 1: {
                // Add Reservation
                char day[MAX_STR], start_time[MAX_STR];
                int duration;
                printf("Enter day (Monday, Tuesday, Wednesday, Thursday): ");
                fgets(day, sizeof(day), stdin);
                day[strcspn(day, "\n")] = 0;
                
                printf("Enter start time (e.g., 09:00): ");
                fgets(start_time, sizeof(start_time), stdin);
                start_time[strcspn(start_time, "\n")] = 0;
                
                printf("Enter duration in minutes (30, 60, 90): ");
                fgets(input, sizeof(input), stdin);
                duration = atoi(input);
                
                if (reserve_slot(&scheduler, day, start_time, duration))
                    printf("Reservation added successfully.\n");
                else
                    printf("Failed to add reservation.\n");
                break;
            }
            case 2: {
                // Add Meeting
                Meeting meeting;
                memset(&meeting, 0, sizeof(meeting));
                // Initialize preferred_hours with -1 by default.
                for (int i = 0; i < 8; i++)
                    meeting.preferred_hours[i] = -1;
                
                printf("Enter meeting name: ");
                fgets(meeting.name, sizeof(meeting.name), stdin);
                meeting.name[strcspn(meeting.name, "\n")] = 0;
                
                printf("Enter meeting type: ");
                fgets(meeting.type, sizeof(meeting.type), stdin);
                meeting.type[strcspn(meeting.type, "\n")] = 0;
                
                printf("Enter duration in minutes (30, 60, or 90): ");
                fgets(input, sizeof(input), stdin);
                int dur_min = atoi(input);
                if (dur_min == 30)
                    meeting.duration = 1;
                else if (dur_min == 60)
                    meeting.duration = 2;
                else if (dur_min == 90)
                    meeting.duration = 3;
                else {
                    printf("Invalid duration. Defaulting to 30 minutes.\n");
                    meeting.duration = 1;
                }
                
                printf("Enter preferred start times (separated by space, e.g., 09:30 10:00) or 'none': ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = 0;
                if (strcasecmp(input, "none") != 0) {
                    char *token = strtok(input, " ");
                    int idx = 0;
                    while (token != NULL && idx < 8) {
                        int slot = find_slot_index(token);
                        if (slot != -1)
                            meeting.preferred_hours[idx++] = slot;
                        token = strtok(NULL, " ");
                    }
                    if (idx < 8)
                        meeting.preferred_hours[idx] = -1;
                }
                
                printf("Enter fixed day (or press enter to skip): ");
                fgets(meeting.fixed_day, sizeof(meeting.fixed_day), stdin);
                meeting.fixed_day[strcspn(meeting.fixed_day, "\n")] = 0;
                
                printf("Enter fixed time (or press enter to skip): ");
                fgets(meeting.fixed_time, sizeof(meeting.fixed_time), stdin);
                meeting.fixed_time[strcspn(meeting.fixed_time, "\n")] = 0;
                
                printf("Enter frequency (weekly, fortnightly, third_week, monthly): ");
                fgets(meeting.frequency, sizeof(meeting.frequency), stdin);
                meeting.frequency[strcspn(meeting.frequency, "\n")] = 0;
                
                if (add_meeting(&scheduler, &meeting))
                    printf("Meeting added successfully.\n");
                else
                    printf("Failed to add meeting.\n");
                break;
            }
            case 3:
                // Display Schedule
                display_schedule(&scheduler);
                break;
            case 4: {
                // Export to ICS
                char filename[128];
                printf("Enter filename for ICS export (e.g., schedule.ics): ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = 0;
                export_to_ics(&scheduler, filename);
                break;
            }
            case 5: {
                // Load Sample Data (as in your original main)
                reserve_slot(&scheduler, "Monday", "14:00", 60);
                reserve_slot(&scheduler, "Wednesday", "15:00", 30);
                Meeting meetings[] = {
                    {"One-to-one with Ian", "one-to-one", 1, {2, 3, 4, 5, 6, 7, -1}, "", "", "weekly"},
                    {"One-to-one with Fari", "one-to-one", 1, {2, 3, 4, 5, 6, 7, -1}, "", "", "weekly"},
                    {"One-to-one with Perith", "one-to-one", 1, {2, 3, 4, 5, 6, 7, -1}, "", "", "weekly"},
                    {"Rotating one-to-one", "one-to-one", 1, {-1}, "", "", "weekly"},
                    {"Weekly Management", "management", 2, {-1}, "Tuesday", "", "weekly"},
                    {"Project All-hands", "management", 2, {4, 5, -1}, "Wednesday", "", "weekly"},
                    {"BIM Review", "management", 2, {-1}, "", "", "fortnightly"},
                    {"Client Update", "client update", 3, {-1}, "", "", "monthly"},
                    {"Contractor Update", "client update", 2, {-1}, "Thursday", "", "weekly"},
                };
                int meeting_count = sizeof(meetings) / sizeof(meetings[0]);
                for (int i = 0; i < meeting_count; i++) {
                    add_meeting(&scheduler, &meetings[i]);
                }
                printf("Sample data loaded.\n");
                break;
            }
            case 6:
                printf("Exiting scheduler.\n");
                return;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
}

int main() {
    srand(time(NULL));
    interactive_menu();
    return 0;
}
