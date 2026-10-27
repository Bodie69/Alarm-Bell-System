// ==========================================
// ESP32 VIRTUAL SYSTEM SIMULATOR ENGINE
// ==========================================

const MATRIX_COLS = 128;
const MATRIX_ROWS = 64;
const PIXEL_SCALE = 4; // Canvas resolution 512x256

let audioEnabled = true;
let audioCtx = null;

// Virtual ESP32 Hardware State
const esp32 = {
    timezoneOffset: 8.0,
    brightness: 70,
    holidayMode: false,
    gpsFix: true,
    satellites: 8,
    isRinging: false,
    activePatternName: "IDLE",
    relayPinState: false,
    manualButtonPressed: false,
    
    // Internal RTC & Timer
    simulatedEpochOffset: 0,
    schoolScrollX: 128,
    lastSchoolScrollTime: 0,
    subjectScrollX: 128,
    lastSubjectScrollTime: 0,
    ringAnimFrame: 0,
    lastRingAnimTime: 0,
    lastSecondChecked: -1,
    
    // Relay pulse queue
    relaySteps: [],
    relayStepIdx: 0,
    relayStepStart: 0,
    
    schedules: [
        { id: 1, dayOfWeek: -1, startHour: 7, startMinute: 30, endHour: 7, endMinute: 45, subjectName: "MORNING ASSEMBLY", ringOnStart: true, ringOnEnd: false, startPattern: 0, endPattern: 0, enabled: true },
        { id: 2, dayOfWeek: -1, startHour: 7, startMinute: 45, endHour: 8, endMinute: 30, subjectName: "PERIOD 1: MATHEMATICS", ringOnStart: true, ringOnEnd: false, startPattern: 0, endPattern: 0, enabled: true },
        { id: 3, dayOfWeek: -1, startHour: 8, startMinute: 30, endHour: 9, endMinute: 15, subjectName: "PERIOD 2: ENGLISH", ringOnStart: true, ringOnEnd: false, startPattern: 0, endPattern: 0, enabled: true },
        { id: 4, dayOfWeek: -1, startHour: 9, startMinute: 15, endHour: 10, endMinute: 0, subjectName: "PERIOD 3: SCIENCE & LAB", ringOnStart: true, ringOnEnd: false, startPattern: 0, endPattern: 0, enabled: true },
        { id: 5, dayOfWeek: -1, startHour: 10, startMinute: 0, endHour: 10, endMinute: 30, subjectName: "RECESS & SNACKS", ringOnStart: true, ringOnEnd: false, startPattern: 1, endPattern: 0, enabled: true },
        { id: 6, dayOfWeek: -1, startHour: 10, startMinute: 30, endHour: 11, endMinute: 15, subjectName: "PERIOD 4: SOCIAL STUDIES", ringOnStart: true, ringOnEnd: false, startPattern: 0, endPattern: 0, enabled: true },
        { id: 7, dayOfWeek: -1, startHour: 11, startMinute: 15, endHour: 12, endMinute: 0, subjectName: "PERIOD 5: COMPUTER LAB", ringOnStart: true, ringOnEnd: false, startPattern: 0, endPattern: 0, enabled: true },
        { id: 8, dayOfWeek: -1, startHour: 12, startMinute: 0, endHour: 13, endMinute: 0, subjectName: "LUNCH BREAK", ringOnStart: true, ringOnEnd: false, startPattern: 1, endPattern: 0, enabled: true },
        { id: 9, dayOfWeek: -1, startHour: 13, startMinute: 0, endHour: 13, endMinute: 45, subjectName: "PERIOD 6: ART & DESIGN", ringOnStart: true, ringOnEnd: false, startPattern: 0, endPattern: 0, enabled: true },
        { id: 10, dayOfWeek: -1, startHour: 13, startMinute: 45, endHour: 14, endMinute: 30, subjectName: "PERIOD 7: PHYSICAL ED", ringOnStart: true, ringOnEnd: true, startPattern: 0, endPattern: 1, enabled: true }
    ]
};

// Canvas & Context
let canvas, ctx;

// Initialization
document.addEventListener('DOMContentLoaded', () => {
    canvas = document.getElementById('matrixCanvas');
    ctx = canvas.getContext('2d');
    
    initTabs();
    renderScheduleTable();
    updateBrightness(esp32.brightness);

    // Main animation loop at 30fps
    requestAnimationFrame(simLoop);

    // Status polling interval
    setInterval(updateTelemetry, 1000);
    setInterval(generateNmeaSentence, 1000);
});

// ==========================================
// AUDIO SYNTHESIZER (Web Audio API)
// ==========================================
function getAudioContext() {
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
        audioCtx.resume();
    }
    return audioCtx;
}

function toggleAudio() {
    audioEnabled = !audioEnabled;
    const btn = document.getElementById('audioToggleBtn');
    btn.innerText = audioEnabled ? '🔊 Audio: ON' : '🔇 Audio: MUTED';
}

function playRelayClickSound(isClosing) {
    if (!audioEnabled) return;
    try {
        const actx = getAudioContext();
        const osc = actx.createOscillator();
        const gain = actx.createGain();
        osc.type = isClosing ? 'square' : 'triangle';
        osc.frequency.setValueAtTime(isClosing ? 800 : 400, actx.currentTime);
        osc.frequency.exponentialRampToValueAtTime(120, actx.currentTime + 0.04);
        gain.gain.setValueAtTime(0.3, actx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.01, actx.currentTime + 0.04);
        osc.connect(gain);
        gain.connect(actx.destination);
        osc.start();
        osc.stop(actx.currentTime + 0.04);
    } catch (e) {}
}

let bellAudioOsc = null;
let bellAudioGain = null;

function startBellChimeSound() {
    if (!audioEnabled) return;
    try {
        const actx = getAudioContext();
        if (bellAudioOsc) stopBellChimeSound();

        const osc1 = actx.createOscillator();
        const osc2 = actx.createOscillator();
        const gain = actx.createGain();

        osc1.type = 'sawtooth';
        osc1.frequency.setValueAtTime(780, actx.currentTime);
        osc2.type = 'sine';
        osc2.frequency.setValueAtTime(1560, actx.currentTime);

        // Tremolo / Hammer vibration
        const tremolo = actx.createOscillator();
        const tremoloGain = actx.createGain();
        tremolo.frequency.setValueAtTime(14, actx.currentTime); // 14 Hz bell hammer strikes
        tremoloGain.gain.setValueAtTime(0.6, actx.currentTime);
        tremolo.connect(tremoloGain.gain);

        gain.gain.setValueAtTime(0.25, actx.currentTime);

        osc1.connect(gain);
        osc2.connect(gain);
        gain.connect(actx.destination);

        osc1.start();
        osc2.start();
        tremolo.start();

        bellAudioOsc = { osc1, osc2, tremolo };
        bellAudioGain = gain;
    } catch (e) {}
}

function stopBellChimeSound() {
    try {
        if (bellAudioOsc) {
            const actx = getAudioContext();
            bellAudioGain.gain.exponentialRampToValueAtTime(0.001, actx.currentTime + 0.2);
            setTimeout(() => {
                try {
                    bellAudioOsc.osc1.stop();
                    bellAudioOsc.osc2.stop();
                    bellAudioOsc.tremolo.stop();
                } catch (e) {}
                bellAudioOsc = null;
            }, 220);
        }
    } catch (e) {}
}

// ==========================================
// SYSTEM TIME & SCHEDULE EVALUATION
// ==========================================
function getSimulatedDate() {
    const now = new Date(Date.now() + esp32.simulatedEpochOffset);
    // Apply UTC offset
    const utc = now.getTime() + (now.getTimezoneOffset() * 60000);
    const targetDate = new Date(utc + (3600000 * esp32.timezoneOffset));
    return targetDate;
}

function evaluateCurrentPeriod(dt) {
    if (esp32.holidayMode) {
        return { hasActive: false, subject: "HOLIDAY MODE", remMinutes: 0, remSeconds: 0, progress: 0 };
    }

    const curDay = dt.getDay(); // 0=Sun, 1=Mon...
    const nowSec = (dt.getHours() * 3600) + (dt.getMinutes() * 60) + dt.getSeconds();

    for (const item of esp32.schedules) {
        if (!item.enabled) continue;
        if (item.dayOfWeek === -1 && (curDay < 1 || curDay > 5)) continue;
        if (item.dayOfWeek >= 0 && item.dayOfWeek !== curDay) continue;

        const startSec = (item.startHour * 3600) + (item.startMinute * 60);
        const endSec = (item.endHour * 3600) + (item.endMinute * 60);

        if (nowSec >= startSec && nowSec < endSec) {
            const diff = endSec - nowSec;
            const total = endSec - startSec;
            return {
                hasActive: true,
                subject: item.subjectName,
                remMinutes: Math.floor(diff / 60),
                remSeconds: diff % 60,
                progress: ((nowSec - startSec) / total) * 100
            };
        }
    }
    return { hasActive: false, subject: "STANDBY / FREE TIME", remMinutes: 0, remSeconds: 0, progress: 0 };
}

function evaluateNextPeriod(dt) {
    if (esp32.holidayMode) return { hasNext: false, subject: "NONE", startHour: 0, startMinute: 0, minsUntil: 0 };

    const curDay = dt.getDay();
    const nowMin = (dt.getHours() * 60) + dt.getMinutes();
    let closestDiff = 99999;
    let closestItem = null;

    for (const item of esp32.schedules) {
        if (!item.enabled) continue;
        if (item.dayOfWeek === -1 && (curDay < 1 || curDay > 5)) continue;
        if (item.dayOfWeek >= 0 && item.dayOfWeek !== curDay) continue;

        const startMin = (item.startHour * 60) + item.startMinute;
        if (startMin > nowMin) {
            const diff = startMin - nowMin;
            if (diff < closestDiff) {
                closestDiff = diff;
                closestItem = item;
            }
        }
    }

    if (closestItem) {
        return {
            hasNext: true,
            subject: closestItem.subjectName,
            startHour: closestItem.startHour,
            startMinute: closestItem.startMinute,
            minsUntil: closestDiff
        };
    }
    return { hasNext: false, subject: "NONE TODAY", startHour: 0, startMinute: 0, minsUntil: 0 };
}

// ==========================================
// RELAY ACTUATOR TIMING STATE MACHINE
// ==========================================
function triggerRelayPattern(pattern, customDurationMs = 4000) {
    esp32.isRinging = true;
    esp32.relayStepIdx = 0;
    esp32.relayStepStart = Date.now();

    switch (pattern) {
        case 1: // DOUBLE
            esp32.activePatternName = "DOUBLE RING";
            esp32.relaySteps = [
                { state: true, dur: 1800 },
                { state: false, dur: 800 },
                { state: true, dur: 1800 }
            ];
            break;
        case 2: // TRIPLE WARNING
            esp32.activePatternName = "WARNING CHIME";
            esp32.relaySteps = [
                { state: true, dur: 500 },
                { state: false, dur: 300 },
                { state: true, dur: 500 },
                { state: false, dur: 300 },
                { state: true, dur: 500 }
            ];
            break;
        case 3: // CUSTOM
            esp32.activePatternName = "CUSTOM RING";
            esp32.relaySteps = [{ state: true, dur: customDurationMs }];
            break;
        case 0:
        default: // SINGLE
            esp32.activePatternName = "SINGLE RING";
            esp32.relaySteps = [{ state: true, dur: customDurationMs || 4000 }];
            break;
    }

    applyRelayHardwareState(esp32.relaySteps[0].state);
}

function stopRelay() {
    esp32.isRinging = false;
    esp32.activePatternName = "IDLE";
    esp32.relaySteps = [];
    applyRelayHardwareState(false);
}

function applyRelayHardwareState(state) {
    const changed = (esp32.relayPinState !== state);
    esp32.relayPinState = state;

    const coil = document.getElementById('relayCoil');
    const led = document.getElementById('relayLed');
    const dot = document.getElementById('relayHwDot');
    const stateText = document.getElementById('relayStateText');
    const gong = document.getElementById('bellDome');

    if (state) {
        coil.classList.add('active');
        led.classList.add('on');
        dot.classList.remove('off');
        dot.style.background = '#ef4444';
        dot.style.boxShadow = '0 0 10px #ef4444';
        stateText.innerText = 'ACTIVE (ON)';
        gong.classList.add('ringing');
        if (changed) {
            playRelayClickSound(true);
            startBellChimeSound();
        }
    } else {
        coil.classList.remove('active');
        led.classList.remove('on');
        dot.style.background = '#10b981';
        dot.style.boxShadow = '0 0 8px #10b981';
        stateText.innerText = 'OFF';
        gong.classList.remove('ringing');
        if (changed) {
            playRelayClickSound(false);
            stopBellChimeSound();
        }
    }
}

function updateRelayStateMachine() {
    if (!esp32.isRinging || esp32.relaySteps.length === 0) return;

    const currentStep = esp32.relaySteps[esp32.relayStepIdx];
    if (Date.now() - esp32.relayStepStart >= currentStep.dur) {
        esp32.relayStepIdx++;
        if (esp32.relayStepIdx < esp32.relaySteps.length) {
            esp32.relayStepStart = Date.now();
            applyRelayHardwareState(esp32.relaySteps[esp32.relayStepIdx].state);
        } else {
            stopRelay();
        }
    }
}

// ==========================================
// 128x64 HUB75 LED MATRIX CANVAS RENDERER
// ==========================================
function simLoop() {
    updateRelayStateMachine();
    renderMatrixDisplay();
    requestAnimationFrame(simLoop);
}

function renderMatrixDisplay() {
    // Clear canvas
    ctx.fillStyle = '#030508';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    const dt = getSimulatedDate();
    const current = evaluateCurrentPeriod(dt);
    const next = evaluateNextPeriod(dt);

    // If relay is actively ringing, draw flashing bell alert screen
    if (esp32.isRinging) {
        renderMatrixRingingAlert();
    } else {
        renderMatrixSchoolBanner();
        renderMatrixMidTime(dt);
        renderMatrixBottom(dt, current, next);
    }
}

function renderMatrixSchoolBanner() {
    // Header background (y: 0 to 16)
    ctx.fillStyle = '#0f172a';
    ctx.fillRect(0, 0, 128 * PIXEL_SCALE, 16 * PIXEL_SCALE);

    const schoolText = "PANAGINTINGAN ELEMENTARY SCHOOL";
    ctx.font = 'bold 16px monospace';
    const textWidth = ctx.measureText(schoolText).width / PIXEL_SCALE;

    if (Date.now() - esp32.lastSchoolScrollTime > 35) {
        esp32.schoolScrollX -= 1;
        if (esp32.schoolScrollX < -(textWidth + 10)) {
            esp32.schoolScrollX = 128;
        }
        esp32.lastSchoolScrollTime = Date.now();
    }

    ctx.save();
    ctx.beginPath();
    ctx.rect(0, 0, 128 * PIXEL_SCALE, 16 * PIXEL_SCALE);
    ctx.clip();

    ctx.fillStyle = '#ffdc00';
    ctx.fillText(schoolText, esp32.schoolScrollX * PIXEL_SCALE, 13 * PIXEL_SCALE);
    ctx.restore();
}

function renderMatrixMidTime(dt) {
    let hour12 = dt.getHours() % 12;
    if (hour12 === 0) hour12 = 12;
    const ampm = dt.getHours() >= 12 ? 'PM' : 'AM';

    const hStr = String(hour12).padStart(2, '0');
    const mStr = String(dt.getMinutes()).padStart(2, '0');

    // Time Digits in Size 4 (height 32, y: 16 to 48) in Green
    ctx.font = 'bold 30px monospace';
    ctx.fillStyle = '#00ff50';

    // Hours
    ctx.fillText(hStr, 2 * PIXEL_SCALE, 42 * PIXEL_SCALE);

    // Colon dots in Green
    ctx.fillRect(49 * PIXEL_SCALE, 23 * PIXEL_SCALE, 4 * PIXEL_SCALE, 4 * PIXEL_SCALE);
    ctx.fillRect(49 * PIXEL_SCALE, 37 * PIXEL_SCALE, 4 * PIXEL_SCALE, 4 * PIXEL_SCALE);

    // Minutes in Green
    ctx.fillText(mStr, 55 * PIXEL_SCALE, 42 * PIXEL_SCALE);

    // AM/PM in Size 2 in Green
    ctx.font = 'bold 16px monospace';
    ctx.fillStyle = '#00ff50';
    ctx.fillText(ampm, 104 * PIXEL_SCALE, 34 * PIXEL_SCALE);

    // GPS Status Indicator Icon at bottom-right (x=118, y=39)
    const gx = 118 * PIXEL_SCALE;
    const gy = 39 * PIXEL_SCALE;
    if (esp32.gpsFix) {
        ctx.fillStyle = '#00ff50';
        ctx.fillRect(gx + 4, gy, 8, 3);
        ctx.fillRect(gx + 2, gy + 3, 12, 3);
        ctx.fillRect(gx + 4, gy + 6, 8, 5);
    } else {
        const blink = Math.floor(Date.now() / 500) % 2 === 0;
        ctx.fillStyle = blink ? '#ff8c00' : '#334155';
        ctx.beginPath();
        ctx.arc(gx + 6, gy + 4, 4, 0, Math.PI * 2);
        ctx.fill();
    }
}

function renderMatrixBottom(dt, current, next) {
    const days = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
    const dayStr = days[dt.getDay()];

    let subjStr = "";
    let subjColor = '#00ff50';

    if (esp32.holidayMode) {
        subjStr = "[Holiday Mode]";
        subjColor = '#ff8c00';
    } else if (current.hasActive) {
        subjStr = current.subject;
        subjColor = '#00ff50';
    } else if (next.hasNext) {
        let nH = next.startHour % 12;
        if (nH === 0) nH = 12;
        const nAp = next.startHour >= 12 ? 'PM' : 'AM';
        const sm = String(next.startMinute).padStart(2, '0');
        subjStr = `Next: ${nH}:${sm} ${nAp} ${next.subject}`;
        subjColor = '#c83cff';
    } else {
        subjStr = "Free Time";
        subjColor = '#00f0ff';
    }

    const bottomText = `${dayStr} | ${subjStr}`;

    ctx.font = 'bold 16px monospace';
    ctx.fillStyle = subjColor;
    const textWidth = ctx.measureText(bottomText).width / PIXEL_SCALE;

    ctx.save();
    ctx.beginPath();
    ctx.rect(0, 48 * PIXEL_SCALE, 128 * PIXEL_SCALE, 16 * PIXEL_SCALE);
    ctx.clip();

    if (Date.now() - esp32.lastSubjectScrollTime > 35) {
        esp32.subjectScrollX -= 1;
        if (esp32.subjectScrollX < -(textWidth + 10)) {
            esp32.subjectScrollX = 128;
        }
        esp32.lastSubjectScrollTime = Date.now();
    }
    ctx.fillText(bottomText, esp32.subjectScrollX * PIXEL_SCALE, 61 * PIXEL_SCALE);
    ctx.restore();
}

function renderMatrixRingingAlert() {
    if (Date.now() - esp32.lastRingAnimTime > 120) {
        esp32.ringAnimFrame = (esp32.ringAnimFrame + 1) % 4;
        esp32.lastRingAnimTime = Date.now();
    }

    const borderCol = (esp32.ringAnimFrame % 2 === 0) ? '#ff1e1e' : '#ffdc00';
    ctx.strokeStyle = borderCol;
    ctx.lineWidth = 4 * PIXEL_SCALE;
    ctx.strokeRect(2 * PIXEL_SCALE, 2 * PIXEL_SCALE, (128 - 4) * PIXEL_SCALE, (64 - 4) * PIXEL_SCALE);

    // Title
    ctx.fillStyle = (esp32.ringAnimFrame % 2 === 0) ? '#ffffff' : '#ffdc00';
    ctx.font = 'bold 11px monospace';
    ctx.fillText("*** BELL RINGING ***", 12 * PIXEL_SCALE, 14 * PIXEL_SCALE);

    // Soundwaves & Bell Icon
    const cx = 64 * PIXEL_SCALE;
    const cy = 32 * PIXEL_SCALE;
    ctx.fillStyle = '#ffdc00';
    ctx.font = '24px monospace';
    ctx.fillText("🔔", cx - 14, cy + 8);

    // Pattern Name
    ctx.fillStyle = '#00ff50';
    ctx.font = 'bold 11px monospace';
    const patText = esp32.activePatternName;
    const pWidth = ctx.measureText(patText).width;
    ctx.fillText(patText, (canvas.width - pWidth) / 2, 54 * PIXEL_SCALE);
}

// ==========================================
// TELEMETRY & WEB PORTAL LOGIC
// ==========================================
function updateTelemetry() {
    const dt = getSimulatedDate();
    const current = evaluateCurrentPeriod(dt);
    const next = evaluateNextPeriod(dt);

    // Update Clock & Date in portal
    const h = String(dt.getHours()).padStart(2, '0');
    const m = String(dt.getMinutes()).padStart(2, '0');
    const s = String(dt.getSeconds()).padStart(2, '0');
    document.getElementById('liveClock').innerText = `${h}:${m}:${s}`;

    const days = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"];
    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
    document.getElementById('liveDate').innerText = `${days[dt.getDay()]} ${dt.getDate()} ${months[dt.getMonth()]} ${dt.getFullYear()}`;
    document.getElementById('satCount').innerText = esp32.satellites;
    document.getElementById('tzOffset').innerText = (esp32.timezoneOffset >= 0 ? '+' : '') + esp32.timezoneOffset;

    // GPS Badge
    const gpsDot = document.getElementById('gpsStatusBadge').querySelector('.status-dot');
    const gpsText = document.getElementById('gpsStatusText');
    if (esp32.gpsFix) {
        gpsDot.className = 'status-dot locked';
        gpsText.innerText = `GPS Locked (${esp32.satellites} Sats)`;
    } else {
        gpsDot.className = 'status-dot searching';
        gpsText.innerText = `GPS Searching (${esp32.satellites} Sats)`;
    }

    // Current & Next
    if (current.hasActive) {
        document.getElementById('curSubject').innerText = current.subject;
        document.getElementById('curTimer').innerText = `Time Remaining: ${current.remMinutes}m ${current.remSeconds}s`;
        document.getElementById('periodProgress').style.width = `${Math.min(100, Math.max(0, current.progress))}%`;
    } else {
        document.getElementById('curSubject').innerText = esp32.holidayMode ? 'HOLIDAY MODE' : 'STANDBY / BREAK';
        document.getElementById('curTimer').innerText = 'No class currently in session';
        document.getElementById('periodProgress').style.width = '0%';
    }

    if (next.hasNext) {
        const sh = String(next.startHour).padStart(2, '0');
        const sm = String(next.startMinute).padStart(2, '0');
        document.getElementById('nextSubject').innerText = next.subject;
        document.getElementById('nextTime').innerText = `@ ${sh}:${sm} (in ${next.minsUntil}m)`;
    } else {
        document.getElementById('nextSubject').innerText = 'None';
        document.getElementById('nextTime').innerText = '--:--';
    }

    // Ringing Banner
    const ringingBanner = document.getElementById('ringingBanner');
    if (esp32.isRinging) {
        ringingBanner.classList.remove('hidden');
        document.getElementById('ringingPatternText').innerText = `Pattern: ${esp32.activePatternName}`;
    } else {
        ringingBanner.classList.add('hidden');
    }

    // Automatic Bell Triggering check at minute edge (00 seconds)
    if (dt.getSeconds() === 0 && dt.getSeconds() !== esp32.lastSecondChecked) {
        checkAutomatedScheduleTrigger(dt);
    }
    esp32.lastSecondChecked = dt.getSeconds();
}

function checkAutomatedScheduleTrigger(dt) {
    if (esp32.holidayMode) return;
    const curDay = dt.getDay();
    const ch = dt.getHours();
    const cm = dt.getMinutes();

    for (const item of esp32.schedules) {
        if (!item.enabled) continue;
        if (item.dayOfWeek === -1 && (curDay < 1 || curDay > 5)) continue;
        if (item.dayOfWeek >= 0 && item.dayOfWeek !== curDay) continue;

        if (item.ringOnStart && ch === item.startHour && cm === item.startMinute) {
            triggerRelayPattern(item.startPattern, 4000);
            showToast(`🔔 Automated Bell: Starting ${item.subjectName}`);
            return;
        }
        if (item.ringOnEnd && ch === item.endHour && cm === item.endMinute) {
            triggerRelayPattern(item.endPattern, 4000);
            showToast(`🔔 Automated Bell: Ending ${item.subjectName}`);
            return;
        }
    }
}

function generateNmeaSentence() {
    const dt = getSimulatedDate();
    const h = String(dt.getUTCHours()).padStart(2, '0');
    const m = String(dt.getUTCMinutes()).padStart(2, '0');
    const s = String(dt.getUTCSeconds()).padStart(2, '0');
    const d = String(dt.getUTCDate()).padStart(2, '0');
    const mo = String(dt.getUTCMonth() + 1).padStart(2, '0');
    const y = String(dt.getUTCFullYear()).substring(2);

    const fixChar = esp32.gpsFix ? 'A' : 'V';
    const log = document.getElementById('nmeaLog');
    log.innerText = `$GPRMC,${h}${m}${s}.00,${fixChar},0118.234,N,10351.456,E,0.02,24.1,${d}${mo}${y},,,A*7B`;
}

// Hardware Workbench Controls
function toggleGpsLock() {
    esp32.gpsFix = !esp32.gpsFix;
    const btn = document.getElementById('toggleGpsFixBtn');
    const dot = document.getElementById('gpsHwDot');
    if (esp32.gpsFix) {
        btn.innerText = 'Toggle Fix: LOCKED';
        btn.className = 'btn btn-sm btn-primary';
        dot.className = 'hw-status-dot';
    } else {
        btn.innerText = 'Toggle Fix: SEARCHING';
        btn.className = 'btn btn-sm btn-warning';
        dot.className = 'hw-status-dot searching';
    }
    updateTelemetry();
}

function updateSatellites(val) {
    esp32.satellites = parseInt(val, 10);
    document.getElementById('gpsSatVal').innerText = val;
    updateTelemetry();
}

function updateBrightness(val) {
    esp32.brightness = parseInt(val, 10);
    const pct = Math.round((esp32.brightness / 255) * 100);
    document.getElementById('hwBrightnessLabel').innerText = `${pct}%`;
    canvas.style.opacity = Math.max(0.2, esp32.brightness / 255);
}

function updateTimezone(val) {
    esp32.timezoneOffset = val;
    updateTelemetry();
    showToast(`Timezone updated to UTC ${val >= 0 ? '+' : ''}${val}:00`);
}

function toggleHolidayMode(enabled) {
    esp32.holidayMode = enabled;
    updateTelemetry();
    showToast(enabled ? '🌴 Holiday Mode Enabled (Bells Muted)' : '📚 Normal School Schedule Active');
}

function pressManualButton() {
    triggerRelayPattern(0, 3000);
    showToast('🔘 GPIO 21 Manual Button Pressed!');
}

function releaseManualButton() {}

// Tab UI
function initTabs() {
    const tabs = document.querySelectorAll('.tab-btn');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            tabs.forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            tab.classList.add('active');
            const targetId = tab.getAttribute('data-tab');
            const el = document.getElementById(targetId);
            if (el) el.classList.add('active');
        });
    });

    const hash = window.location.hash.replace('#', '');
    if (hash) {
        const targetBtn = document.querySelector(`.tab-btn[data-tab="${hash}"]`);
        if (targetBtn) targetBtn.click();
    }

    const params = new URLSearchParams(window.location.search);
    if (params.get('ring') === '1') {
        setTimeout(() => triggerRelayPattern(0, 15000), 200);
    }
    if (params.get('add') === '1') {
        setTimeout(() => addNewScheduleRow(), 300);
    }
    if (params.get('save') === '1') {
        setTimeout(() => saveAllSchedules(), 400);
    }
}

function renderScheduleTable() {
    const tbody = document.getElementById('scheduleTableBody');
    tbody.innerHTML = '';

    esp32.schedules.forEach((item, index) => {
        const tr = document.createElement('tr');
        const sh = String(item.startHour).padStart(2, '0');
        const sm = String(item.startMinute).padStart(2, '0');
        const eh = String(item.endHour).padStart(2, '0');
        const em = String(item.endMinute).padStart(2, '0');

        tr.innerHTML = `
            <td>
                <select class="form-control" onchange="updateScheduleField(${index}, 'dayOfWeek', parseInt(this.value))">
                    <option value="-1" ${item.dayOfWeek === -1 ? 'selected' : ''}>Mon - Fri (Weekdays)</option>
                    <option value="-2" ${item.dayOfWeek === -2 ? 'selected' : ''}>Everyday</option>
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
            <td><button class="btn btn-danger btn-sm" onclick="deleteScheduleRow(${index})">🗑️</button></td>
        `;
        tbody.appendChild(tr);
    });
}

function handleTimeChange(index, type, val) {
    if (!val) return;
    const parts = val.split(':');
    if (type === 'start') {
        esp32.schedules[index].startHour = parseInt(parts[0], 10);
        esp32.schedules[index].startMinute = parseInt(parts[1], 10);
    } else {
        esp32.schedules[index].endHour = parseInt(parts[0], 10);
        esp32.schedules[index].endMinute = parseInt(parts[1], 10);
    }
}

function updateScheduleField(index, field, val) {
    esp32.schedules[index][field] = val;
}

function addNewScheduleRow() {
    esp32.schedules.push({
        id: Date.now() % 10000,
        dayOfWeek: -1,
        startHour: 8,
        startMinute: 0,
        endHour: 8,
        endMinute: 45,
        subjectName: "NEW CLASS",
        ringOnStart: true,
        ringOnEnd: false,
        startPattern: 0,
        endPattern: 0,
        enabled: true
    });
    renderScheduleTable();
    showToast('+ Added new timetable period');
}

function deleteScheduleRow(index) {
    esp32.schedules.splice(index, 1);
    renderScheduleTable();
    showToast('Deleted timetable period');
}

function saveAllSchedules() {
    showToast('💾 Schedules saved to simulated LittleFS flash!');
}

function triggerBell(pattern, duration) {
    triggerRelayPattern(pattern, duration);
}

function triggerCustomBell() {
    const durSec = parseInt(document.getElementById('customDurationSlider').value, 10);
    triggerRelayPattern(3, durSec * 1000);
}

function stopBell() {
    stopRelay();
    showToast('🛑 Bell stopped.');
}

function syncBrowserTimeToEsp() {
    esp32.simulatedEpochOffset = 0;
    showToast('🕒 Time calibrated from browser!');
    updateTelemetry();
}

function showToast(msg) {
    const toast = document.getElementById('toast');
    toast.innerText = msg;
    toast.classList.remove('hidden');
    setTimeout(() => { toast.classList.add('hidden'); }, 3000);
}
