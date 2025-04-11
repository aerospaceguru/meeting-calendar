import calendar
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from datetime import datetime

# Set up calendar for May 2025
cal = calendar.monthcalendar(2025, 5)
month_name = "May 2025"
days_in_week = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]

# Define holidays
sa_holidays = [(5, 1)]  # Workers' Day: May 1
uk_holidays = [(5, 5), (5, 26)]  # Early May: May 5, Spring: May 26

# Define meeting schedule with UK times only (GMT+1, BST)
meetings = {
    # Week 1
    (5, 2): ["Ian 1:1 10:00 AM", "Roll-A 1:1 9:30 AM", "BIM Review 11:00 AM", "Lagan Brief 10:00 AM", "Client Update 13:00 PM"],
    # Week 2
    (5, 6): ["Fari 1:1 10:00 AM", "Management 14:00 PM"],
    (5, 7): ["Perith 1:1 10:00 AM", "All-Hands 11:00 AM"],
    (5, 8): ["Lagan Brief 10:00 AM"],
    (5, 9): ["Roll-C 1:1 9:30 AM"],
    # Week 3
    (5, 12): ["Ian 1:1 10:00 AM", "Roll-D 1:1 9:30 AM"],
    (5, 13): ["Fari 1:1 10:00 AM", "Management 14:00 PM"],
    (5, 14): ["Perith 1:1 10:00 AM", "All-Hands 11:00 AM", "Client Review 13:00 PM"],
    (5, 15): ["Lagan Brief 10:00 AM"],
    (5, 16): ["Roll-E 1:1 9:30 AM", "BIM Review 11:00 AM"],
    # Week 4
    (5, 19): ["Ian 1:1 10:00 AM", "Roll-F 1:1 9:30 AM"],
    (5, 20): ["Fari 1:1 10:00 AM", "Management 14:00 PM"],
    (5, 21): ["Perith 1:1 10:00 AM", "All-Hands 11:00 AM"],
    (5, 22): ["Lagan Brief 10:00 AM"],
    (5, 23): ["Roll-G 1:1 9:30 AM"],
    # Week 5
    (5, 27): ["Fari 1:1 10:00 AM", "Management 14:00 PM"],
    (5, 28): ["Perith 1:1 10:00 AM", "All-Hands 11:00 AM"],
    (5, 29): ["Lagan Brief 10:00 AM"],
    (5, 30): ["Roll-H 1:1 9:30 AM", "BIM Review 11:00 AM"],
}

# Define unique colors for each meeting type and corresponding text colors for contrast
meeting_colors = {
    "Ian 1:1": "blue",         # Blue for Ian 1:1
    "Fari 1:1": "darkgreen",   # Dark Green for Fari 1:1
    "Perith 1:1": "purple",    # Purple for Perith 1:1
    "Role-A 1:1": "orange",    # Orange for Roll-A
    "Role-B 1:1": "teal",      # Teal for Roll-B
    "Role-C 1:1": "brown",     # Brown for Roll-C
    "Role-D 1:1": "pink",      # Pink for Roll-D
    "Role-E 1:1": "olive",     # Olive for Roll-E
    "Role-F 1:1": "cyan",      # Cyan for Roll-F
    "Role-G 1:1": "magenta",   # Magenta for Roll-G
    "All-Hands": "green",      # Green for All-Hands
    "Lagan Brief": "darkorange",  # Dark Orange for Lagan Brief
    "Management": "red",       # Red for Management
    "BIM Review": "darkcyan",  # Dark Cyan for BIM Review
    "Client Review": "darkviolet",  # Dark Violet for Client Review
    "Client Update": "navy",   # Navy for Client Update
}

# Define text colors for contrast against the fill color
text_colors = {
    "blue": "white",       # White text on blue
    "darkgreen": "white",  # White text on dark green
    "purple": "white",     # White text on purple
    "orange": "black",     # Black text on orange
    "teal": "white",       # White text on teal
    "brown": "white",      # White text on brown
    "pink": "black",       # Black text on pink
    "olive": "white",      # White text on olive
    "cyan": "black",       # Black text on cyan
    "magenta": "white",    # White text on magenta
    "green": "white",      # White text on green
    "darkorange": "black", # Black text on dark orange
    "red": "white",        # White text on red
    "darkcyan": "white",   # White text on dark cyan
    "darkviolet": "white", # White text on dark violet
    "navy": "white",       # White text on navy
    
    # IMPORTANT: Make black bars have white text
    "black": "white"       
}

# Create figure and axis
fig, ax = plt.subplots(figsize=(16, 12))
ax.set_title(month_name + " - Airfield Rehabilitation Project", fontsize=16, pad=20)

# Set up grid
ax.set_xticks(range(7))
ax.set_xticklabels(days_in_week, fontsize=12)
ax.set_yticks(range(len(cal)))
ax.set_yticklabels([f"Week {i+1}" for i in range(len(cal))], fontsize=12)

# Customize grid
ax.grid(True, which="both", linestyle="--", linewidth=0.5)
ax.set_xlim(-0.5, 6.5)
ax.set_ylim(-0.5, len(cal) - 0.5)
ax.invert_yaxis()  # Top-left is Monday, Week 1

# Function to convert time to minutes past 9:00 AM
def time_to_minutes(time_str):
    # Extract time from the meeting string (e.g., "10:00 AM" from "Ian 1:1 10:00 AM")
    time_part = time_str.split()[-2] + " " + time_str.split()[-1]  # e.g., "10:00 AM"
    time_obj = datetime.strptime(time_part, "%H:%M %p")
    hours = time_obj.hour
    minutes = time_obj.minute
    # Convert to minutes past midnight
    total_minutes = hours * 60 + minutes
    # Adjust for 9:00 AM start (9:00 AM = 540 minutes past midnight)
    minutes_past_9am = total_minutes - (9 * 60)
    if minutes_past_9am < 0:  # Handle times before 9:00 AM (unlikely, but for robustness)
        minutes_past_9am = 0
    if minutes_past_9am > 480:  # Handle times after 5:00 PM (480 minutes = 8 hours)
        minutes_past_9am = 480
    return minutes_past_9am

# Plot days
for week_idx, week in enumerate(cal):
    for day_idx, day in enumerate(week):
        if day == 0:  # No day in this slot
            ax.add_patch(mpatches.Rectangle((day_idx - 0.5, week_idx - 0.5), 1, 1, color="lightgrey"))
            continue

        # Determine cell color (white for all days except holidays)
        cell_color = "white"  # Default: all days are white
        if (5, day) in sa_holidays or (5, day) in uk_holidays:
            cell_color = "salmon"  # Holiday

        # Draw cell
        ax.add_patch(
            mpatches.Rectangle((day_idx - 0.5, week_idx - 0.5), 1, 1,
                               facecolor=cell_color, edgecolor="black")
        )

        # Add day number in the top-left corner (avoid overlap)
        ax.text(day_idx - 0.48, week_idx - 0.48, str(day),
                ha="left", va="top", fontsize=12, weight="bold", zorder=5)

        # Add holiday label
        if (5, day) in sa_holidays:
            ax.text(day_idx, week_idx + 0.4, "SA: Workers’ Day", ha="center", va="bottom", 
                    fontsize=8, color="darkred", wrap=True)
        elif (5, day) in uk_holidays:
            ax.text(day_idx, week_idx + 0.4, "UK: Bank Holiday", ha="center", va="bottom", 
                    fontsize=8, color="darkred", wrap=True)

        # Add meetings with time-based positioning and filled boxes
        if (5, day) in meetings:
            meeting_list = meetings[(5, day)]
            for meeting in meeting_list:
                # Extract the meeting type (full prefix, e.g., "Roll-A 1:1")
                meeting_type = " ".join(meeting.split(" ")[:2])  # e.g., "Ian 1:1", "Roll-A 1:1"
                fill_color = meeting_colors.get(meeting_type, "black")
                text_color = text_colors.get(fill_color, "white")  # Default text = white if not found

                # Calculate position based on time (9:00 AM to 5:00 PM = 1.0 height)
                minutes_past_9am = time_to_minutes(meeting)
                # Map minutes to y-position (0 to 480 minutes -> 0 to 1.0 in cell)
                y_offset = (minutes_past_9am / 480.0)  # 480 minutes = 8 hours
                y_position = (week_idx - 0.5) + y_offset

                # Add meeting text with a filled box
                ax.text(
                    day_idx, y_position, meeting,
                    ha="center", va="center", fontsize=6,
                    color=text_color,
                    bbox=dict(facecolor=fill_color, edgecolor="black", boxstyle="round,pad=0.2"),
                    wrap=True
                )

# Adjust layout (no legend needed here)
plt.tight_layout()

# Save and show
plt.savefig("may_2025_calendar_updated.png", dpi=300, bbox_inches="tight")
plt.show()
