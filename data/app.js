// State
let schedules = [];
let isHolidayMode = false;
let settingsInitialized = false; // guards against clobbering in-progress edits during the 1s poll
let isPollingStatus = false;
let currentServerSeconds = -1; // seconds since midnight
let lastSyncTimestamp = 0;

function initLocalClockTick() {
    setInterval(() => {
        if (currentServerSeconds >= 0) {
            const elapsed = Math.floor((Date.now() - lastSyncTimestamp) / 1000);
            const totalSec = (currentServerSeconds + elapsed) % 86400;
            const h = Math.floor(totalSec / 3600);
            const m = Math.floor((totalSec % 3600) / 60);
            const s = totalSec % 60;
            const timeStr = `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
            const clockText = formatTo12Hour(timeStr);
            const el1 = document.getElementById('liveClock');
            const el2 = document.getElementById('headerTime');
            if (el1 && el1.innerText !== '-- Not Synced --' && el1.innerText !== '--:--:-- --') el1.innerText = clockText;
            if (el2 && el2.innerText !== '--:--:-- --') el2.innerText = clockText;
        }
    }, 1000);
}

async function pollStatusLoop() {
    if (!document.hidden && !isPollingStatus) {
        isPollingStatus = true;
        try {
            await fetchStatus();
        } catch (e) {
            // Ignored, next cycle will retry
        } finally {
            isPollingStatus = false;
        }
    }
    setTimeout(pollStatusLoop, 2500); // 2.5 second polite polling
}

document.addEventListener('DOMContentLoaded', () => {
    // Capture token from URL query params (e.g. redirected from login)
    const urlParams = new URLSearchParams(window.location.search);
    const urlToken = urlParams.get('token');
    if (urlToken) {
        localStorage.setItem('session_token', urlToken);
        document.cookie = `session=${urlToken}; path=/; max-age=28800`;
        window.history.replaceState({}, document.title, '/');
    }

    initTabs();
    initSidebarToggle();
    initLocalClockTick();

    // Stagger initial calls to prevent ESP32 socket congestion
    fetchStatus().then(() => {
        setTimeout(fetchSchedules, 300);
        setTimeout(fetchActivity, 700);
        setTimeout(pollStatusLoop, 2500);
    }).catch(() => {
        setTimeout(fetchSchedules, 500);
        setTimeout(pollStatusLoop, 2500);
    });

    // Refresh activity log every 15 seconds
    setInterval(() => {
        if (!document.hidden) fetchActivity();
    }, 15000);
});

// Sidebar navigation (tab switching)
function initTabs() {
    const tabs = document.querySelectorAll('.nav-link');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            tabs.forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

            tab.classList.add('active');
            const targetId = tab.getAttribute('data-tab');
            const targetEl = document.getElementById(targetId);
            if (targetEl) targetEl.classList.add('active');

            closeSidebar(); // no-op on desktop widths, closes the mobile drawer
        });
    });

    const hash = window.location.hash.replace('#', '');
    if (hash) {
        const targetBtn = document.querySelector(`.nav-link[data-tab="${hash}"]`);
        if (targetBtn) targetBtn.click();
    }
}

// Mobile off-canvas sidebar (hamburger menu)
function initSidebarToggle() {
    const sidebar = document.getElementById('sidebar');
    const overlay = document.getElementById('sidebarOverlay');
    const openBtn = document.getElementById('sidebarToggleBtn');
    if (!sidebar || !overlay || !openBtn) return;

    openBtn.addEventListener('click', () => {
        sidebar.classList.add('open');
        overlay.classList.add('visible');
    });
    overlay.addEventListener('click', closeSidebar);
}

function closeSidebar() {
    const sidebar = document.getElementById('sidebar');
    const overlay = document.getElementById('sidebarOverlay');
    if (sidebar) sidebar.classList.remove('open');
    if (overlay) overlay.classList.remove('visible');
}

// Wraps fetch() so an expired/missing session bounces the user back to the
// login page instead of leaving the dashboard silently non-functional.
async function apiFetch(url, options = {}) {
    options.headers = options.headers || {};
    let token = localStorage.getItem('session_token');
    if (token) {
        options.headers['Authorization'] = 'Bearer ' + token;
        const sep = url.includes('?') ? '&' : '?';
        url = `${url}${sep}token=${encodeURIComponent(token)}`;
    }
    if (!options.signal && typeof AbortSignal !== 'undefined' && AbortSignal.timeout) {
        options.signal = AbortSignal.timeout(4000);
    }
    const res = await fetch(url, options);
    if (res.status === 401) {
        localStorage.removeItem('session_token');
        window.location.href = '/login.html';
        throw new Error('Not authenticated');
    }
    return res;
}

async function logout() {
    try {
        await apiFetch('/api/logout', { method: 'POST' });
    } catch (e) {
        // apiFetch already redirects on 401; ignore network errors here too —
        // we're leaving this page regardless.
    }
    localStorage.removeItem('session_token');
    window.location.href = '/login.html';
}

// ---- Formatting helpers ----

// "14:05:30" (24h, from the firmware) -> "2:05:30 PM"
function formatTo12Hour(timeStr) {
    if (!timeStr) return '--:--:-- --';
    const [hStr, mStr, sStr] = timeStr.split(':');
    let h = parseInt(hStr, 10);
    const ampm = h >= 12 ? 'PM' : 'AM';
    h = h % 12;
    if (h === 0) h = 12;
    return `${h}:${mStr}:${sStr} ${ampm}`;
}

// hour (0-23), minute -> "2:05 PM"
function formatHM12(hour, minute) {
    let h = hour % 12;
    if (h === 0) h = 12;
    const ampm = hour >= 12 ? 'PM' : 'AM';
    return `${h}:${String(minute).padStart(2, '0')} ${ampm}`;
}

function formatCountdown(totalSeconds) {
    const clamped = Math.max(0, totalSeconds);
    const h = Math.floor(clamped / 3600);
    const m = Math.floor((clamped % 3600) / 60);
    const s = clamped % 60;
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

// Seconds from "now" (HH:MM:SS, today) until the given hour:minute today.
function secondsUntil(nowTimeStr, targetHour, targetMinute) {
    const [nh, nm, ns] = nowTimeStr.split(':').map(Number);
    const nowSec = (nh * 3600) + (nm * 60) + ns;
    const targetSec = (targetHour * 3600) + (targetMinute * 60);
    let diff = targetSec - nowSec;
    if (diff < 0) diff += 86400; // shouldn't happen for "next" bells, but stay safe
    return diff;
}

// Backend sends a large sentinel (~9999999ms) when the clock has never been synced.
function formatSyncAge(ms) {
    if (typeof ms !== 'number' || ms > 9000000) return 'Never';
    const totalSec = Math.floor(ms / 1000);
    if (totalSec < 60) return `${totalSec}s ago`;
    const min = Math.floor(totalSec / 60);
    if (min < 60) return `${min}m ago`;
    const hr = Math.floor(min / 60);
    return `${hr}h ${min % 60}m ago`;
}

function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str == null ? '' : String(str);
    return div.innerHTML;
}

// Fetch Status
async function fetchStatus() {
    try {
        const res = await apiFetch('/api/status');
        if (!res.ok) return;
        const data = await res.json();

        // ---- Header clock/date ----
        // timeValid reflects whether the RTC has ever been set (GPS fix or
        // manual sync) — distinct from gpsFix, which is only about a *live*
        // signal right now. A clock synced minutes ago and still accurate
        // shouldn't be shown as broken just because the fix dropped.
        const timeValid = !!data.timeValid;

        if (timeValid) {
            if (data.time) {
                const parts = data.time.split(':').map(Number);
                if (parts.length >= 3) {
                    currentServerSeconds = (parts[0] * 3600) + (parts[1] * 60) + parts[2];
                    lastSyncTimestamp = Date.now();
                }
            }
            const clockText = formatTo12Hour(data.time);
            const dateText = `${data.day || ''} ${data.date || ''}`;
            document.getElementById('liveClock').innerText = clockText;
            document.getElementById('liveDate').innerText = dateText;
            document.getElementById('headerTime').innerText = clockText;
            document.getElementById('headerDate').innerText = dateText;
            document.getElementById('rtcSyncText').innerHTML = '<span style="color:var(--green);">✓ Synced</span>';
        } else {
            document.getElementById('liveClock').innerText = '-- Not Synced --';
            document.getElementById('liveDate').innerText = 'Waiting for GPS fix or manual sync';
            document.getElementById('headerTime').innerText = '--:--:-- --';
            document.getElementById('headerDate').innerText = 'Awaiting time sync';
            document.getElementById('rtcSyncText').innerHTML = '<span style="color:var(--red);">✗ Not Synced</span>';
        }

        // ---- Next Bell card ----
        if (!timeValid) {
            document.getElementById('nextBellTime').innerText = '--:--';
            document.getElementById('nextBellPeriod').innerText = 'Waiting for time sync';
            document.getElementById('nextBellCountdown').innerHTML = '&nbsp;';
        } else if (data.next && data.next.hasNext) {
            document.getElementById('nextBellTime').innerText = formatHM12(data.next.startHour, data.next.startMinute);
            document.getElementById('nextBellPeriod').innerText = data.next.subject;
            const secsLeft = secondsUntil(data.time, data.next.startHour, data.next.startMinute);
            document.getElementById('nextBellCountdown').innerText = `Starts in ${formatCountdown(secsLeft)}`;
        } else {
            document.getElementById('nextBellTime').innerText = '--:--';
            document.getElementById('nextBellPeriod').innerText = data.holidayMode ? 'Holiday Mode — bells muted' : 'No upcoming bell today';
            document.getElementById('nextBellCountdown').innerHTML = '&nbsp;';
        }

        // ---- System Status card (GPS line) ----
        document.getElementById('gpsConnectedStatus').innerHTML = data.gpsFix
            ? `GPS: <span style="color:var(--green);">✓ Connected</span> (${data.satellites} sats)`
            : 'GPS: <span style="color:var(--text-muted);">Searching...</span>';

        // ---- GPS Status page ----
        const gpsFixEl = document.getElementById('gpsFixValue');
        const gpsFixSubEl = document.getElementById('gpsFixSub');
        if (gpsFixEl && gpsFixSubEl) {
            gpsFixEl.innerText = data.gpsFix ? 'GPS Locked' : 'Searching';
            gpsFixEl.style.color = data.gpsFix ? 'var(--green)' : 'var(--amber)';
            gpsFixSubEl.innerText = data.gpsFix ? 'Fix acquired' : 'Waiting for satellite fix';
        }
        const gpsSatEl = document.getElementById('gpsSatValue');
        if (gpsSatEl) gpsSatEl.innerText = data.satellites || 0;
        const gpsHdopEl = document.getElementById('gpsHdopValue');
        if (gpsHdopEl) gpsHdopEl.innerText = (typeof data.hdop === 'number' && data.hdop < 99) ? data.hdop.toFixed(1) : '--';
        const gpsTzEl = document.getElementById('gpsTzValue');
        if (gpsTzEl) gpsTzEl.innerText = (data.timezone >= 0 ? '+' : '') + data.timezone + ':00';
        const gpsLastSyncEl = document.getElementById('gpsLastSyncValue');
        if (gpsLastSyncEl) gpsLastSyncEl.innerText = formatSyncAge(data.lastSyncAgeMs);

        // Populate Settings tab controls from the device's actual saved state,
        // but only once — otherwise the 1s poll would fight the user's cursor
        // mid-drag on the sliders.
        if (!settingsInitialized) {
            document.getElementById('tzSelect').value = String(data.timezone);
            document.getElementById('matrixBrightness').value = data.brightness || 90;
            settingsInitialized = true;
        }

        // ---- Today's Bell Schedule table ----
        renderTodaySchedule(data);

        // ---- Ringing Banner ----
        const ringingBanner = document.getElementById('ringingBanner');
        if (data.isRinging) {
            ringingBanner.classList.remove('hidden');
        } else {
            ringingBanner.classList.add('hidden');
        }
    } catch (e) {
        console.warn('Telemetry poll error:', e);
    }
}

// ---- Today's Bell Schedule (Dashboard) ----
const DAY_NAMES = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'];

function isDayMatchingToday(scheduleDay, currentDayNum) {
    if (scheduleDay === -1) return currentDayNum >= 1 && currentDayNum <= 5; // weekdays
    if (scheduleDay === -2) return true; // everyday
    return scheduleDay === currentDayNum;
}

function renderTodaySchedule(data) {
    const tbody = document.getElementById('todayScheduleBody');
    if (!tbody) return;

    if (!data.timeValid) {
        tbody.innerHTML = '<tr class="table-empty-row"><td colspan="3">Waiting for the system clock to sync (GPS fix or manual sync)...</td></tr>';
        return;
    }
    if (!data.day || !data.time) return;

    const dayNum = DAY_NAMES.indexOf(data.day);
    const [nh, nm, ns] = data.time.split(':').map(Number);
    const nowSec = (nh * 3600) + (nm * 60) + ns;

    const todays = schedules
        .filter(item => item.enabled && isDayMatchingToday(item.dayOfWeek, dayNum))
        .map(item => ({
            ...item,
            startSec: (item.startHour * 3600) + (item.startMinute * 60),
            endSec: (item.endHour * 3600) + (item.endMinute * 60)
        }))
        .sort((a, b) => a.startSec - b.startSec);

    if (todays.length === 0) {
        tbody.innerHTML = '<tr class="table-empty-row"><td colspan="3">No periods scheduled for today.</td></tr>';
        return;
    }

    let nextAssigned = false;
    tbody.innerHTML = todays.map(item => {
        let statusHtml;
        let rowClass = '';

        if (nowSec >= item.endSec) {
            statusHtml = '<span class="status-pill done">✅ Done</span>';
        } else if (nowSec >= item.startSec) {
            statusHtml = '<span class="status-pill active-now">▶ In Session</span>';
        } else if (!nextAssigned) {
            statusHtml = '<span class="status-pill next">⏳ Next</span>';
            rowClass = 'row-next';
            nextAssigned = true;
        } else {
            statusHtml = '<span class="status-pill pending">○ Pending</span>';
        }

        const timeStr = `${formatHM12(item.startHour, item.startMinute)} – ${formatHM12(item.endHour, item.endMinute)}`;
        return `<tr class="${rowClass}"><td>${escapeHtml(item.subjectName)}</td><td>${timeStr}</td><td>${statusHtml}</td></tr>`;
    }).join('');
}

// ---- Recent Bell Activity ----
async function fetchActivity() {
    try {
        const res = await apiFetch('/api/activity');
        if (!res.ok) return;
        const data = await res.json();
        renderActivityTable(data.events || []);
    } catch (e) {
        console.warn('Fetch activity error:', e);
    }
}

function renderActivityTable(events) {
    const tbody = document.getElementById('activityTableBody');
    if (!tbody) return;

    if (events.length === 0) {
        tbody.innerHTML = '<tr class="table-empty-row"><td colspan="2">No activity recorded yet.</td></tr>';
        return;
    }

    tbody.innerHTML = events.map(ev =>
        `<tr><td>${escapeHtml(ev.time)}</td><td>${escapeHtml(ev.activity)}</td></tr>`
    ).join('');
}

// Fetch Schedules
async function fetchSchedules() {
    try {
        const res = await apiFetch('/api/schedule');
        if (!res.ok) return;
        const data = await res.json();
        schedules = data.schedules || [];
        isHolidayMode = data.holidayMode || false;
        
        document.getElementById('holidayModeCheck').checked = isHolidayMode;
        renderScheduleTable();
    } catch (e) {
        console.error('Fetch schedule error:', e);
    }
}

// Render Schedule Table
function renderScheduleTable() {
    const tbody = document.getElementById('scheduleTableBody');
    tbody.innerHTML = '';

    schedules.forEach((item, index) => {
        const tr = document.createElement('tr');
        
        const sh = String(item.startHour).padStart(2, '0');
        const sm = String(item.startMinute).padStart(2, '0');
        const eh = String(item.endHour).padStart(2, '0');
        const em = String(item.endMinute).padStart(2, '0');

        tr.innerHTML = `
            <td>
                <select class="form-control" onchange="updateScheduleField(${index}, 'dayOfWeek', parseInt(this.value))">
                    <option value="-1" ${item.dayOfWeek === -1 ? 'selected' : ''}>Mon - Fri (Weekdays)</option>
                    <option value="-2" ${item.dayOfWeek === -2 ? 'selected' : ''}>Everyday (Mon - Sun)</option>
                    <option value="1" ${item.dayOfWeek === 1 ? 'selected' : ''}>Monday</option>
                    <option value="2" ${item.dayOfWeek === 2 ? 'selected' : ''}>Tuesday</option>
                    <option value="3" ${item.dayOfWeek === 3 ? 'selected' : ''}>Wednesday</option>
                    <option value="4" ${item.dayOfWeek === 4 ? 'selected' : ''}>Thursday</option>
                    <option value="5" ${item.dayOfWeek === 5 ? 'selected' : ''}>Friday</option>
                    <option value="6" ${item.dayOfWeek === 6 ? 'selected' : ''}>Saturday</option>
                    <option value="0" ${item.dayOfWeek === 0 ? 'selected' : ''}>Sunday</option>
                </select>
            </td>
            <td><input type="time" value="${sh}:${sm}" class="form-control" onchange="handleTimeChange(${index}, 'start', this.value)"></td>
            <td><input type="time" value="${eh}:${em}" class="form-control" onchange="handleTimeChange(${index}, 'end', this.value)"></td>
            <td><input type="text" value="${item.subjectName || ''}" class="form-control" maxlength="30" onchange="updateScheduleField(${index}, 'subjectName', this.value)"></td>
            <td>
                <label style="display:flex;align-items:center;gap:4px;">
                    <input type="checkbox" ${item.ringOnStart ? 'checked' : ''} onchange="updateScheduleField(${index}, 'ringOnStart', this.checked)">
                    <span>Ring</span>
                </label>
            </td>
            <td>
                <label style="display:flex;align-items:center;gap:4px;">
                    <input type="checkbox" ${item.ringOnEnd ? 'checked' : ''} onchange="updateScheduleField(${index}, 'ringOnEnd', this.checked)">
                    <span>Ring</span>
                </label>
            </td>
            <td>
                <button class="btn btn-danger" onclick="deleteScheduleRow(${index})">🗑️</button>
            </td>
        `;
        tbody.appendChild(tr);
    });
}

function handleTimeChange(index, type, val) {
    if (!val) return;
    const parts = val.split(':');
    const h = parseInt(parts[0], 10);
    const m = parseInt(parts[1], 10);
    if (type === 'start') {
        schedules[index].startHour = h;
        schedules[index].startMinute = m;
    } else {
        schedules[index].endHour = h;
        schedules[index].endMinute = m;
    }
}

function updateScheduleField(index, field, val) {
    schedules[index][field] = val;
}

function addNewScheduleRow() {
    schedules.push({
        id: Date.now() % 10000,
        dayOfWeek: -1,
        startHour: 8,
        startMinute: 0,
        endHour: 8,
        endMinute: 45,
        subjectName: "NEW SUBJECT",
        ringOnStart: true,
        ringOnEnd: false,
        enabled: true
    });
    renderScheduleTable();
}

function deleteScheduleRow(index) {
    schedules.splice(index, 1);
    renderScheduleTable();
}

// Returns an error string describing the first invalid row, or null if all rows are valid.
function validateSchedules() {
    for (let i = 0; i < schedules.length; i++) {
        const item = schedules[i];
        const name = (item.subjectName || '').trim();
        if (name.length === 0) {
            return `Row ${i + 1}: Subject name cannot be empty.`;
        }
        const startMin = (item.startHour * 60) + item.startMinute;
        const endMin = (item.endHour * 60) + item.endMinute;
        if (endMin <= startMin) {
            return `Row ${i + 1} (${name}): End time must be after start time.`;
        }
    }
    return null;
}

async function saveAllSchedules() {
    const validationError = validateSchedules();
    if (validationError) {
        showToast(`❌ ${validationError}`);
        return;
    }

    try {
        const payload = {
            holidayMode: document.getElementById('holidayModeCheck').checked,
            schedules: schedules
        };
        const res = await apiFetch('/api/schedule', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (res.ok) {
            showToast('✅ Schedules saved successfully to LittleFS flash!');
        } else {
            const errBody = await res.json().catch(() => null);
            showToast(`❌ ${errBody && errBody.message ? errBody.message : 'Failed to save schedules.'}`);
        }
    } catch (e) {
        showToast('❌ Network error while saving.');
    }
}

// Manual Bell Triggers
// Every trigger — scheduled or manual — rings for a fixed 5 seconds; there's
// no pattern or duration to choose.
async function triggerBell() {
    try {
        await apiFetch('/api/ring', { method: 'POST' });
        showToast('🔔 Bell triggered!');
        setTimeout(fetchActivity, 300); // let the backend log the event before we poll
    } catch (e) {
        showToast('❌ Error triggering bell');
    }
}

async function stopBell() {
    try {
        await apiFetch('/api/stop', { method: 'POST' });
        showToast('🛑 Bell stopped.');
    } catch (e) {
        showToast('❌ Error stopping bell');
    }
}

// System Settings
async function saveSystemSettings() {
    const tz = parseFloat(document.getElementById('tzSelect').value);
    const brightness = parseInt(document.getElementById('matrixBrightness').value, 10);
    const holiday = document.getElementById('holidayModeCheck').checked;

    try {
        const res = await apiFetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                timezone: tz,
                brightness: brightness,
                holidayMode: holiday
            })
        });
        if (res.ok) {
            showToast('⚙️ System settings updated!');
        }
    } catch (e) {
        showToast('❌ Failed to update settings.');
    }
}

// Admin Credentials
async function changeCredentials() {
    const currentPassword = document.getElementById('currentPasswordInput').value;
    const newUsername = document.getElementById('newUsernameInput').value.trim();
    const newPassword = document.getElementById('newPasswordInput').value;
    const confirmPassword = document.getElementById('confirmPasswordInput').value;

    if (!currentPassword) {
        showToast('❌ Enter your current password.');
        return;
    }
    if (newPassword && newPassword !== confirmPassword) {
        showToast('❌ New passwords do not match.');
        return;
    }
    if (newPassword && newPassword.length < 4) {
        showToast('❌ New password must be at least 4 characters.');
        return;
    }

    try {
        const res = await apiFetch('/api/auth/change', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ currentPassword, newUsername, newPassword })
        });
        const body = await res.json().catch(() => null);
        if (res.ok) {
            showToast('✅ Credentials updated. Use the new login next time.');
            document.getElementById('currentPasswordInput').value = '';
            document.getElementById('newPasswordInput').value = '';
            document.getElementById('confirmPasswordInput').value = '';
        } else {
            showToast(`❌ ${body && body.message ? body.message : 'Failed to update credentials.'}`);
        }
    } catch (e) {
        showToast('❌ Network error while updating credentials.');
    }
}

// Sync Browser Time to ESP32
async function syncBrowserTimeToEsp() {
    // Send true UTC (epoch seconds) — never decomposed local Y/M/D/H/M/S
    // fields, which would silently assume the browser and the ESP32 are
    // configured to the same timezone and set the clock wrong if they
    // aren't. The backend converts using its own configured offset, the
    // same way GPS sync does.
    const payload = { utcEpochSeconds: Math.floor(Date.now() / 1000) };

    try {
        const res = await apiFetch('/api/timesync', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (res.ok) {
            showToast('🕒 Time synchronized with browser clock!');
        }
    } catch (e) {
        showToast('❌ Error syncing time.');
    }
}

function showToast(msg) {
    const toast = document.getElementById('toast');
    toast.innerText = msg;
    toast.classList.remove('hidden');
    setTimeout(() => {
        toast.classList.add('hidden');
    }, 3500);
}
