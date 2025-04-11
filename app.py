from flask import Flask, render_template, request, redirect, url_for, session
import math
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import io
import base64

app = Flask(__name__)
app.secret_key = 'your_secret_key'  # Change this!

# Default durations for each meeting type (still used for other purposes if needed)
DEFAULT_DURATIONS = {
    "One-to-One Meeting": 30,
    "All-Hands Meeting": 60,
    "Management Meeting": 60,
    "BIM Review": 45,
    "Client Update": 30,
    "Client Review": 45,
    "Lagan Brief": 20,
}

# Colors for meeting types and text contrast
MEETING_COLORS = {
    "One-to-One Meeting": "blue",
    "All-Hands Meeting": "green",
    "Management Meeting": "red",
    "BIM Review": "darkcyan",
    "Client Update": "navy",
    "Client Review": "darkviolet",
    "Lagan Brief": "darkorange",
}
TEXT_COLORS = {
    "blue": "white",
    "green": "white",
    "red": "white",
    "darkcyan": "white",
    "navy": "white",
    "darkviolet": "white",
    "darkorange": "black",
}

# Recurrence options delta values (in days)
# Monthly meetings occur only once.
RECURRENCE_DELTA = {
    "Weekly": 7,
    "Fortnightly": 14,
    "Every Third Week": 21,
    "Monthly": None,
}

# Fixed visual height for each meeting bar (in minutes, relative to the 9:00–5:00 = 480 minute day)
FIXED_BAR_DURATION = 30  # All bars are drawn with the height equivalent to 30 minutes

@app.route('/', methods=['GET', 'POST'])
def add_meeting():
    """
    Renders the meeting request form.
    The form collects:
      - Meeting Type (dropdown starting with a "select meeting type" placeholder)
      - Time Slot (Morning or Afternoon)
      - Recurrence Pattern (Weekly, Fortnightly, Every Third Week, Monthly)
      - Preferred Day (optional; if set, the meeting begins on the earliest instance of that day)
      - Person's Name (if the meeting type is One-to-One Meeting)
    """
    if 'meetings' not in session:
        session['meetings'] = []
    if request.method == 'POST':
        meeting_type = request.form.get("meeting_type")
        time_slot = request.form.get("time_slot")  # "Morning" or "Afternoon"
        recurrence = request.form.get("recurrence")
        preferred_day = request.form.get("preferred_day")
        person_name = request.form.get("person_name")
        
        # Do not proceed if meeting type is not chosen.
        if not meeting_type:
            return redirect(url_for('add_meeting'))
        
        # Generate an order based on submission order
        meetings = session.get('meetings', [])
        order = len(meetings) + 1
        
        meeting = {
            "meeting_type": meeting_type,
            "time_slot": time_slot,
            "recurrence": recurrence,
            "preferred_day": preferred_day,
            "person_name": person_name,
            "order": order
        }
        meetings.append(meeting)
        session['meetings'] = meetings
        return redirect(url_for('add_meeting'))
    return render_template("add_meeting.html", meetings=session.get('meetings', []))

@app.route('/calendar')
def calendar_view():
    """
    Generates the monthly schedule as an image.
    
    • Meetings are scheduled only on weekdays (Mon–Fri); weekends are grayed out.
    • For each meeting request, if a Preferred Day is provided, the scheduler picks the earliest instance of that weekday in the month.
      Otherwise, it uses load-balancing (with Fridays slightly penalized).
    • Recurring meetings are scheduled by adding 7, 14, or 21 days, keeping the same weekday.
    • In each day cell, meetings in the Morning (9:00–12:00) or Afternoon (12:00–5:00)
      are assigned a discrete start time (only on the hour or half‑hour) from a fixed list.
    • Every meeting is drawn with a constant fixed height and its label shows the meeting’s start time.
    """
    meeting_requests = session.get('meetings', [])
    
    # Helper: in our generic month, day 1 is Monday.
    def is_weekday(day):
        return (day - 1) % 7 < 5

    def is_friday(day):
        return (day - 1) % 7 == 4

    # List available weekdays
    available_days = [day for day in range(1, 32) if is_weekday(day)]
    day_events = {day: [] for day in available_days}
    day_workload = {day: 0 for day in available_days}
    
    # Process each meeting request.
    for meeting in meeting_requests:
        rec = meeting["recurrence"]
        delta = RECURRENCE_DELTA.get(rec)
        preferred = meeting.get("preferred_day")
        # If a preferred day is provided, choose the earliest instance of that weekday.
        if preferred:
            mapping = {"Monday": 0, "Tuesday": 1, "Wednesday": 2, "Thursday": 3, "Friday": 4}
            target = mapping.get(preferred)
            filtered = [d for d in available_days if (d - 1) % 7 == target]
            if filtered:
                base_day = min(filtered)  # Earliest instance on that day
            else:
                base_day = min(available_days)
        else:
            base_day = min(available_days, key=lambda d: ((1 if is_friday(d) else 0), day_workload[d]))
        
        occurrences = []
        if delta is None:
            # Monthly: one occurrence.
            if base_day <= 31:
                occurrences.append(base_day)
                day_workload[base_day] += 1
        else:
            occ = 0
            candidate_day = base_day + occ * delta
            while candidate_day <= 31:
                if is_weekday(candidate_day):
                    occurrences.append(candidate_day)
                    day_workload[candidate_day] += 1
                occ += 1
                candidate_day = base_day + occ * delta
        
        # For each occurrence, create an event.
        for occ_day in occurrences:
            mt = meeting["meeting_type"]
            ts = meeting["time_slot"]
            pname = meeting.get("person_name", "")
            base_text = f"1:1 with {pname}" if mt == "One-to-One Meeting" else mt
            ev = {
                "meeting_type": mt,
                "time_slot": ts,
                "base_text": base_text,
                "color": MEETING_COLORS.get(mt, "black"),
                "start": None,  # Will be assigned below.
                "order": meeting.get("order", 0)  # Use provided order, fallback to 0 if missing
            }
            day_events[occ_day].append(ev)
    
    # Define fixed valid start times (in minutes from 9:00 AM) for each slot.
    morning_valid = [0, 30, 60, 90, 120, 150]      # 9:00, 9:30, ... 11:30
    afternoon_valid = list(range(180, 451, 30))       # 12:00 to 4:30 (last start so that the bar fits before 5:00)

    # For each day, assign start times for each meeting, ensuring consistent ordering.
    for day, events in day_events.items():
        groups = {"Morning": [], "Afternoon": []}
        for ev in events:
            groups[ev["time_slot"]].append(ev)
        for slot, ev_list in groups.items():
            if not ev_list:
                continue
            valid_slots = morning_valid if slot == "Morning" else afternoon_valid
            n = len(ev_list)
            m = len(valid_slots)
            if n == 1:
                ev_list[0]["start"] = valid_slots[m // 2]
            else:
                # Sort events by their permanent order so that repeated meetings get the same slot each week.
                sorted_events = sorted(ev_list, key=lambda e: e.get("order", 0))
                for i, ev in enumerate(sorted_events):
                    index = round(i * (m - 1) / (n - 1))
                    ev["start"] = valid_slots[index]
    
    # Create a generic 31-day grid. (7 columns: Mon–Sun; weekends are grayed out.)
    total_days = 31
    cols = 7
    rows = math.ceil(total_days / cols)
    
    fig, ax = plt.subplots(figsize=(16, 12))
    ax.set_title("Optimized 31-Day Project Calendar (Meetings: Mon–Fri)", fontsize=16, pad=20)
    
    # Helper: format meeting time from minutes after 9:00 AM.
    def format_time(minutes_after_9):
        total = int(9 * 60 + minutes_after_9)
        hour = total // 60
        minute = total % 60
        suffix = "AM" if hour < 12 else "PM"
        display_hour = hour if hour <= 12 else hour - 12
        if display_hour == 0:
            display_hour = 12
        return f"{display_hour}:{minute:02d} {suffix}"
    
    # Draw the calendar cells.
    for day in range(1, total_days + 1):
        idx = day - 1
        row_idx = idx // cols
        col_idx = idx % cols
        cell_x = col_idx - 0.5
        cell_y = row_idx - 0.5
        cell_color = "lightgrey" if col_idx in [5, 6] else "white"
        ax.add_patch(mpatches.Rectangle((cell_x, cell_y), 1, 1,
                                        facecolor=cell_color, edgecolor="black"))
        ax.text(cell_x + 0.05, cell_y + 0.95, str(day),
                ha="left", va="top", fontsize=10, weight="bold")
        
        # Draw meeting events for this day.
        if day in day_events:
            for ev in day_events[day]:
                bar_height = FIXED_BAR_DURATION / 480.0  # Fixed height across all bars.
                global_x = cell_x  # Bar spans the full width.
                global_y = cell_y + ev["start"] / 480.0
                meeting_time_str = format_time(ev["start"])
                full_text = f"{ev['base_text']} @ {meeting_time_str}"
                ax.add_patch(mpatches.Rectangle((global_x, global_y), 1, bar_height,
                                                facecolor=ev["color"], edgecolor="black", lw=0.5))
                ax.text(global_x + 0.5, global_y + bar_height / 2,
                        full_text,
                        ha="center", va="center", fontsize=8,
                        color=TEXT_COLORS.get(ev["color"], "white"))
    
    ax.set_xticks(range(cols))
    ax.set_xticklabels(["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"], fontsize=12)
    ax.set_yticks(range(rows))
    ax.set_yticklabels([f"Week {i+1}" for i in range(rows)], fontsize=12)
    ax.set_xlim(-0.5, cols - 0.5)
    ax.set_ylim(-0.5, rows - 0.5)
    ax.invert_yaxis()
    ax.grid(True, which="both", linestyle="--", linewidth=0.5)
    
    plt.tight_layout()
    buf = io.BytesIO()
    plt.savefig(buf, format="png", dpi=150, bbox_inches="tight")
    buf.seek(0)
    image_base64 = base64.b64encode(buf.getvalue()).decode('utf-8')
    plt.close(fig)
    return render_template("calendar_result.html", image_base64=image_base64)

@app.route('/clear')
def clear_meetings():
    """Clear all stored meeting requests."""
    session.pop('meetings', None)
    return redirect(url_for('add_meeting'))

if __name__ == '__main__':
    app.run(debug=True)
