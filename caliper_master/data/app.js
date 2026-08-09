// View management functions
function showView(viewId) {
    document.querySelectorAll('.view').forEach(view => {
        view.classList.add('hidden');
    });
    document.getElementById(viewId + '-view').classList.remove('hidden');
}

// Calibration functions
// Single atomic endpoint POST /api/calibrate (set reference + measure + offset=raw).
// "Calibration:" shows the pre-calibration corrected value (previous offset),
// exactly like the GUI toolbar label before sending command 'c'.

let lastCalibration = NaN;
let lastReference = NaN;
let calibrateBusy = false;

function renderCalibrationResult() {
    const el = document.getElementById('calibration-measurement');
    const fmtLine = (v) => Number.isFinite(v) ? v.toFixed(3) + ' mm' : 'n/a';

    el.innerHTML =
        '<div class="cal-line">' +
            '<span class="calibration-line-label">Calibration:</span>' +
            '<span class="calibration-line-value">' + fmtLine(lastCalibration) + '</span>' +
        '</div>' +
        '<div class="cal-line">' +
            '<span class="calibration-line-label">Reference:</span>' +
            '<span class="calibration-line-value">' + fmtLine(lastReference) + '</span>' +
        '</div>';
}

// Initial render: shows "Calibration: n/a" and "Reference: n/a" (like GUI toolbar).
renderCalibrationResult();

function calibrate() {
    if (calibrateBusy) return;

    const elStatus = document.getElementById('cal-status');
    const ref = Number(document.getElementById('reference-input').value);

    if (!Number.isFinite(ref) || ref < -999.999 || ref > 999.999) {
        elStatus.textContent = 'Enter reference (-999.999..999.999) first';
        return;
    }

    calibrateBusy = true;
    elStatus.textContent = 'Calibrating...';

    fetch('/api/calibrate', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'reference=' + ref
    })
    .then(response => response.ok ? response.json()
        : response.json().then(err => { throw new Error(err.error || 'Server error'); }))
    .then(data => {
        if (!data || data.success !== true) {
            throw new Error((data && data.error) ? data.error : 'Unknown error');
        }
        lastCalibration = Number(data.corrected);
        lastReference = Number(data.reference);
        renderCalibrationResult();
        elStatus.textContent = 'OK';
    })
    .catch(error => { elStatus.textContent = 'Error: ' + error.message; })
    .finally(() => { calibrateBusy = false; });
}

/**
 * Session name validation
 * @param {string} name - Session name to validate
 * @returns {boolean} - true if the name is valid
 */
function validateSessionName(name) {
    // Minimum length: 1 character
    if (!name || name.length < 1) {
        return false;
    }

    // Maximum length: 31 characters
    if (name.length > 31) {
        return false;
    }

    // Allowed characters: letters (a-z, A-Z), digits (0-9), spaces, underscores (_), hyphens (-)
    const allowedChars = /^[a-zA-Z0-9 _-]+$/;
    if (!allowedChars.test(name)) {
        return false;
    }

    return true;
}

// Measurement session functions
function startSession() {
    const sessionName = document.getElementById('session-name-input').value;
    
    // Session name validation
    if (!validateSessionName(sessionName)) {
        alert('Session name is invalid (max 31 characters, allowed: a-z, A-Z, 0-9, space, _, -)');
        return;
    }
    
    fetch('/start_session', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'sessionName=' + encodeURIComponent(sessionName)
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Server error'); });
        }
        return response.json();
    })
    .then(data => {
        if (data.error) {
            document.getElementById('status').textContent = 'Error: ' + data.error;
        } else {
            document.getElementById('session-name-display').textContent = data.sessionName || sessionName;
            showView('measurement');
        }
    })
    .catch(error => {
        document.getElementById('status').textContent = 'Error: ' + error.message;
    });
}

function measureSession() {
    document.getElementById('status').textContent = 'Taking measurement...';

    fetch('/measure_session', {
        method: 'POST'
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Server error'); });
        }
        return response.json();
    })
    .then(data => {
        if (data.error) {
            document.getElementById('status').textContent = 'Error: ' + data.error;
            return;
        }

        const isValid = !!data.valid;
        if (!isValid) {
            document.getElementById('measurement-value').textContent = 'No data';
            document.getElementById('measurement-raw').textContent = 'No data';
            document.getElementById('measurement-offset').textContent = 'No data';
            document.getElementById('measurement-reference').textContent = 'No data';
            document.getElementById('battery').textContent = 'No data';
            document.getElementById('angle-z').textContent = 'No data';
            document.getElementById('status').textContent = 'No fresh data (no response from device).';
            return;
        }

        const raw = Number(data.measurementRaw);
        const offset = Number(data.calibrationOffset);
        const ref = Number(data.reference);
        const corrected = (Number.isFinite(raw) && Number.isFinite(offset)) ? (raw - offset) : NaN;
        const finalValue = Number.isFinite(corrected) && Number.isFinite(ref) ? corrected + ref : NaN;

        document.getElementById('measurement-value').textContent = Number.isFinite(finalValue)
            ? finalValue.toFixed(3) + ' mm'
            : (Number.isFinite(corrected) ? corrected.toFixed(3) + ' mm' : (data.measurementCorrected + ' mm'));

        document.getElementById('measurement-raw').textContent = Number.isFinite(raw)
            ? raw.toFixed(3) + ' mm'
            : (data.measurementRaw + ' mm');

        document.getElementById('measurement-offset').textContent = Number.isFinite(offset)
            ? offset.toFixed(3) + ' mm'
            : (data.calibrationOffset + ' mm');

        document.getElementById('measurement-reference').textContent = Number.isFinite(ref)
            ? ref.toFixed(3) + ' mm'
            : (Number.isFinite(data.reference) ? Number(data.reference).toFixed(3) + ' mm' : 'No data');

        const batt = Number(data.batteryVoltage);
        document.getElementById('battery').textContent = Number.isFinite(batt)
            ? batt.toFixed(3) + ' V'
            : (data.batteryVoltage + ' V');

        const angleZ = Number(data.angleZ);
        document.getElementById('angle-z').textContent = Number.isFinite(angleZ)
            ? angleZ.toFixed(2)
            : data.angleZ;

        document.getElementById('status').textContent = 'Updated: ' + new Date().toLocaleTimeString();
    })
    .catch(error => {
        document.getElementById('status').textContent = 'Error: ' + error.message;
    });
}
