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
    document.getElementById('status').textContent = 'Wysyłanie zadania...';
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
        } else {
            document.getElementById('measurement-value').textContent = data.measurement;
            document.getElementById('status').textContent = 'Zadanie wysłane! Odczekaj chwile i odśwież.';
            setTimeout(refreshSession, 1500);
        }
    })
    .catch(error => {
        document.getElementById('status').textContent = 'Błąd: ' + error.message;
    });
}

function refreshSession() {
    document.getElementById('status').textContent = 'Pobieranie danych...';
    
    // Pobierz dane z API
    fetch('/api')
    .then(response => {
        if (!response.ok) {
            throw new Error('Błąd serwera: ' + response.status);
        }
        return response.json();
    })
    .then(data => {
        document.getElementById('measurement-value').textContent = data.measurement;

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

// Funkcje sterowania silnikiem (jeden endpoint /motor → CMD_MOTORTEST)
function motorSend() {
    const state = document.getElementById('motor-state').value;
    const speed = document.getElementById('motor-speed').value;
    const torque = document.getElementById('motor-torque').value;

    document.getElementById('status').textContent = 'Wysyłanie komendy silnika...';

    fetch('/motor', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'state=' + encodeURIComponent(state) +
              '&speed=' + encodeURIComponent(speed) +
              '&torque=' + encodeURIComponent(torque)
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });
        }
        return response.json();
    })
    .then(data => {
        document.getElementById('status').textContent =
            'Motor: state=' + data.state + ' speed=' + data.speed + ' torque=' + data.torque;
    })
    .catch(error => {
        document.getElementById('status').textContent = 'Błąd: ' + error.message;
    });
}

function motorStopQuick() {
    document.getElementById('motor-state').value = '0';
    document.getElementById('motor-speed').value = '0';
    motorSend();
}
