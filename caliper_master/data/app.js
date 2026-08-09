// View management functions
function showView(viewId) {
    document.querySelectorAll('.view').forEach(view => {
        view.classList.add('hidden');
    });
    document.getElementById(viewId + '-view').classList.remove('hidden');
}

// Calibration functions
// Assumption: UI calculates correction on its side:
// corrected = measurementRaw - calibrationOffset

let lastCalibrationRaw = NaN;
let lastCalibrationOffset = NaN;
let lastReference = NaN;
let offsetJustApplied = false;

function formatMm(value) {
    return Number.isFinite(value) ? value.toFixed(3) : 'n/a';
}

function renderCalibrationMeasurement() {
    const elMeas = document.getElementById('calibration-measurement');

    const raw = lastCalibrationRaw;
    const offset = lastCalibrationOffset;
    const ref = lastReference;
    const corrected = (Number.isFinite(raw) && Number.isFinite(offset)) ? (raw - offset) : NaN;
    const finalValue = Number.isFinite(corrected) && Number.isFinite(ref) ? corrected + ref : NaN;

    const offsetLabel = offsetJustApplied ? 'Current offset (applied):' : 'Current offset:';

    elMeas.innerHTML =
        '<div class="cal-line">' +
            '<span class="calibration-line-label">Raw:</span>' +
            '<span class="calibration-line-value">' + formatMm(raw) + ' mm</span>' +
        '</div>' +
        '<div class="cal-line calibration-line--offset">' +
            '<span class="calibration-line-label">' + offsetLabel + '</span>' +
            '<span class="calibration-line-value">' + formatMm(offset) + ' mm</span>' +
        '</div>' +
        '<div class="cal-line">' +
            '<span class="calibration-line-label">Reference:</span>' +
            '<span class="calibration-line-value">' + formatMm(ref) + ' mm</span>' +
        '</div>' +
        '<div class="cal-line">' +
            '<span class="calibration-line-label">Corrected:</span>' +
            '<span class="calibration-line-value">' + formatMm(finalValue) + ' mm</span>' +
        '</div>';
}

function calibrationMeasure() {
    const elStatus = document.getElementById('cal-status');

    elStatus.textContent = 'Fetching current measurement...';

    fetch('/api/calibration/measure', {
        method: 'POST'
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Server error'); });
        }
        return response.json();
    })
    .then(data => {
        if (!data || data.success !== true) {
            throw new Error((data && data.error) ? data.error : 'Unknown error');
        }

        const raw = Number(data.measurementRaw);
        const offset = Number(data.calibrationOffset);
        const ref = Number(data.reference);

        if (Number.isFinite(raw)) {
            // calibration mode: prefill offset field with current measurement (without auto-sending)
            document.getElementById('offset-input').value = raw.toFixed(3);
        }

        lastCalibrationRaw = raw;
        lastCalibrationOffset = offset;
        lastReference = Number.isFinite(ref) ? ref : 0;
        offsetJustApplied = false;

        renderCalibrationMeasurement();

        elStatus.textContent = 'OK';
    })
    .catch(error => {
        elStatus.textContent = 'Error: ' + error.message;
    });
}

function applyCalibrationOffset() {
    const offset = Number(document.getElementById('offset-input').value);
    const elStatus = document.getElementById('cal-status');

    if (!Number.isFinite(offset) || offset < -999.999 || offset > 999.999) {
        alert('Offset must be in range -999.999 .. 999.999');
        return;
    }

    elStatus.textContent = 'Setting offset...';

    fetch('/api/calibration/offset', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'offset=' + offset
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Server error'); });
        }
        return response.json();
    })
    .then(data => {
        if (!data || data.success !== true) {
            throw new Error((data && data.error) ? data.error : 'Unknown error');
        }
        lastCalibrationOffset = Number(data.calibrationOffset);
        offsetJustApplied = true;
        renderCalibrationMeasurement();

        elStatus.textContent = 'OK';
    })
    .catch(error => {
        elStatus.textContent = 'Error: ' + error.message;
    });
}

function applyReference() {
    const ref = Number(document.getElementById('reference-input').value);
    const elStatus = document.getElementById('cal-status');

    if (!Number.isFinite(ref) || ref < -999.999 || ref > 999.999) {
        alert('Reference must be in range -999.999 .. 999.999');
        return;
    }

    elStatus.textContent = 'Setting reference...';

    fetch('/api/reference', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'reference=' + ref
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Server error'); });
        }
        return response.json();
    })
    .then(data => {
        if (!data || data.success !== true) {
            throw new Error((data && data.error) ? data.error : 'Unknown error');
        }
        lastReference = Number(data.reference);
        renderCalibrationMeasurement();

        elStatus.textContent = 'OK';
    })
    .catch(error => {
        elStatus.textContent = 'Error: ' + error.message;
    });
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
