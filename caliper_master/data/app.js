// Funkcje zarządzania widokami
function showView(viewId) {
    document.querySelectorAll('.view').forEach(view => {
        view.classList.add('hidden');
    });
    document.getElementById(viewId + '-view').classList.remove('hidden');
}

// Funkcje kalibracji
function calibrate() {
    const offset = Number(document.getElementById('offset-input').value);
    if (!Number.isFinite(offset) || offset < -14.999 || offset > 14.999) {
        alert('Offset musi być w zakresie -14.999 .. 14.999');
        return;
    }
    document.getElementById('status').textContent = 'Wykonywanie kalibracji...';
    fetch('/calibrate', {
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
        if (data.error && !data.offset) {
            document.getElementById('status').textContent = 'Błąd: ' + data.error;
        } else {
            document.getElementById('calibration-result').innerHTML = 'Offset: ' + data.offset + '<br>Błąd: ' + data.error + ' mm';
            document.getElementById('status').textContent = 'Kalibracja zakończona';
        }
    })
    .catch(error => {
        document.getElementById('status').textContent = 'Błąd: ' + error.message;
    });
}

// Funkcje sesji pomiarowej
function startSession() {
    const sessionName = document.getElementById('session-name-input').value;
    if (!sessionName) {
        alert('Podaj nazwę sesji');
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
            document.getElementById('session-name-display').textContent = sessionName;
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

        document.getElementById('measurement-value').textContent = data.measurement;

        const isValid = !!data.valid;
        if (!isValid) {
            document.getElementById('battery').textContent = 'Brak danych';
            document.getElementById('angle-x').textContent = 'Brak danych';
            document.getElementById('status').textContent = 'Brak świeżych danych (brak odpowiedzi z urządzenia).';
            return;
        }

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
