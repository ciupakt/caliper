// Funkcje zarządzania widokami
function showView(viewId) {
    document.querySelectorAll('.view').forEach(view => {
        view.classList.add('hidden');
    });
    document.getElementById(viewId + '-view').classList.remove('hidden');
}

// Funkcje kalibracji
// Założenie: UI liczy korekcję po swojej stronie:
// corrected = measurementRaw + calibrationOffset

let lastCalibrationRaw = NaN;
let lastCalibrationOffset = NaN;
let offsetJustApplied = false;

function formatMm(value) {
    return Number.isFinite(value) ? value.toFixed(3) : 'n/a';
}

function renderCalibrationMeasurement() {
    const elMeas = document.getElementById('calibration-measurement');

    const raw = lastCalibrationRaw;
    const offset = lastCalibrationOffset;
    const corrected = (Number.isFinite(raw) && Number.isFinite(offset)) ? (raw < 0 ? (raw + offset) : (raw - offset)) : NaN;

    const offsetLabel = offsetJustApplied ? 'Aktualny offset (ustawiono):' : 'Aktualny offset:';

    elMeas.innerHTML =
        '<div class="cal-line">' +
            '<span class="calibration-line-label">Surowy:</span>' +
            '<span class="calibration-line-value">' + formatMm(raw) + ' mm</span>' +
        '</div>' +
        '<div class="cal-line calibration-line--offset">' +
            '<span class="calibration-line-label">' + offsetLabel + '</span>' +
            '<span class="calibration-line-value">' + formatMm(offset) + ' mm</span>' +
        '</div>' +
        '<div class="cal-line">' +
            '<span class="calibration-line-label">Skorygowany:</span>' +
            '<span class="calibration-line-value">' + formatMm(corrected) + ' mm</span>' +
        '</div>';
}

function calibrationMeasure() {
    const elStatus = document.getElementById('cal-status');

    elStatus.textContent = 'Pobieranie bieżącego pomiaru...';

    fetch('/api/calibration/measure', {
        method: 'POST'
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });
        }
        return response.json();
    })
    .then(data => {
        if (!data || data.success !== true) {
            throw new Error((data && data.error) ? data.error : 'Nieznany błąd');
        }

        const raw = Number(data.measurementRaw);
        const offset = Number(data.calibrationOffset);

        if (Number.isFinite(raw)) {
            // tryb kalibracji: podbijamy pole offsetu bieżącym pomiarem (bez automatycznego wysyłania)
            document.getElementById('offset-input').value = raw.toFixed(3);
        }

        lastCalibrationRaw = raw;
        lastCalibrationOffset = offset;
        offsetJustApplied = false;

        renderCalibrationMeasurement();

        elStatus.textContent = 'OK';
    })
    .catch(error => {
        elStatus.textContent = 'Błąd: ' + error.message;
    });
}

function applyCalibrationOffset() {
    const offset = Number(document.getElementById('offset-input').value);
    const elStatus = document.getElementById('cal-status');

    if (!Number.isFinite(offset) || offset < -14.999 || offset > 14.999) {
        alert('Offset musi być w zakresie -14.999 .. 14.999');
        return;
    }

    elStatus.textContent = 'Ustawianie offsetu...';

    fetch('/api/calibration/offset', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'offset=' + offset
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });
        }
        return response.json();
    })
    .then(data => {
        if (!data || data.success !== true) {
            throw new Error((data && data.error) ? data.error : 'Nieznany błąd');
        }
        lastCalibrationOffset = Number(data.calibrationOffset);
        offsetJustApplied = true;
        renderCalibrationMeasurement();

        elStatus.textContent = 'OK';
    })
    .catch(error => {
        elStatus.textContent = 'Błąd: ' + error.message;
    });
}

/**
 * Walidacja nazwy sesji
 * @param {string} name - Nazwa sesji do walidacji
 * @returns {boolean} - true jeśli nazwa jest prawidłowa
 */
function validateSessionName(name) {
    // Minimalna długość: 1 znak
    if (!name || name.length < 1) {
        return false;
    }

    // Maksymalna długość: 31 znaków
    if (name.length > 31) {
        return false;
    }

    // Dozwolone znaki: litery (a-z, A-Z), cyfry (0-9), spacje, podkreślenia (_), myślniki (-)
    const allowedChars = /^[a-zA-Z0-9 _-]+$/;
    if (!allowedChars.test(name)) {
        return false;
    }

    return true;
}

// Funkcje sesji pomiarowej
function startSession() {
    const sessionName = document.getElementById('session-name-input').value;
    
    // Walidacja nazwy sesji
    if (!validateSessionName(sessionName)) {
        alert('Nazwa sesji jest nieprawidłowa (maks 31 znaków, dozwolone: a-z, A-Z, 0-9, spacja, _, -)');
        return;
    }
    
    fetch('/start_session', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'sessionName=' + encodeURIComponent(sessionName)
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });
        }
        return response.json();
    })
    .then(data => {
        if (data.error) {
            document.getElementById('status').textContent = 'Błąd: ' + data.error;
        } else {
            document.getElementById('session-name-display').textContent = data.sessionName || sessionName;
            showView('measurement');
        }
    })
    .catch(error => {
        document.getElementById('status').textContent = 'Błąd: ' + error.message;
    });
}

function measureSession() {
    document.getElementById('status').textContent = 'Wykonywanie pomiaru...';

    fetch('/measure_session', {
        method: 'POST'
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });
        }
        return response.json();
    })
    .then(data => {
        if (data.error) {
            document.getElementById('status').textContent = 'Błąd: ' + data.error;
            return;
        }

        const isValid = !!data.valid;
        if (!isValid) {
            document.getElementById('measurement-value').textContent = 'Brak danych';
            document.getElementById('measurement-raw').textContent = 'Brak danych';
            document.getElementById('measurement-offset').textContent = 'Brak danych';
            document.getElementById('battery').textContent = 'Brak danych';
            document.getElementById('angle-x').textContent = 'Brak danych';
            document.getElementById('status').textContent = 'Brak świeżych danych (brak odpowiedzi z urządzenia).';
            return;
        }

        const raw = Number(data.measurementRaw);
        const offset = Number(data.calibrationOffset);
        const corrected = (Number.isFinite(raw) && Number.isFinite(offset)) ? (raw < 0 ? (raw + offset) : (raw - offset)) : NaN;

        document.getElementById('measurement-value').textContent = Number.isFinite(corrected)
            ? corrected.toFixed(3) + ' mm'
            : (data.measurementCorrected + ' mm');

        document.getElementById('measurement-raw').textContent = Number.isFinite(raw)
            ? raw.toFixed(3) + ' mm'
            : (data.measurementRaw + ' mm');

        document.getElementById('measurement-offset').textContent = Number.isFinite(offset)
            ? offset.toFixed(3) + ' mm'
            : (data.calibrationOffset + ' mm');

        const batt = Number(data.batteryVoltage);
        document.getElementById('battery').textContent = Number.isFinite(batt)
            ? batt.toFixed(3) + ' V'
            : (data.batteryVoltage + ' V');

        const angleX = Number(data.angleX);
        document.getElementById('angle-x').textContent = Number.isFinite(angleX)
            ? angleX.toFixed(2)
            : data.angleX;

        document.getElementById('status').textContent = 'Zaktualizowano: ' + new Date().toLocaleTimeString();
    })
    .catch(error => {
        document.getElementById('status').textContent = 'Błąd: ' + error.message;
    });
}
