document.getElementById('loginForm').addEventListener('submit', async (e) => {
    e.preventDefault();

    const username = document.getElementById('loginUsername').value.trim();
    const password = document.getElementById('loginPassword').value;
    const errBox = document.getElementById('loginError');
    const submitBtn = document.getElementById('loginSubmitBtn');

    errBox.classList.add('hidden');
    submitBtn.disabled = true;
    submitBtn.innerText = 'Signing In...';

    try {
        const formData = new URLSearchParams();
        formData.append('username', username);
        formData.append('password', password);

        const res = await fetch('/api/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: formData.toString()
        });

        const body = await res.json().catch(() => null);

        if (res.ok) {
            if (body && body.token) {
                localStorage.setItem('session_token', body.token);
                document.cookie = `session=${body.token}; path=/; max-age=28800`;
                // Crucial: pass token in query param to bypass captive portal cookie loss
                window.location.href = `/?token=${encodeURIComponent(body.token)}`;
            } else {
                window.location.href = '/';
            }
            return;
        }
        errBox.innerText = (body && body.message) ? body.message : 'Invalid username or password.';
        errBox.classList.remove('hidden');
    } catch (err) {
        errBox.innerText = 'Network error — could not reach the ESP32.';
        errBox.classList.remove('hidden');
    } finally {
        submitBtn.disabled = false;
        submitBtn.innerText = 'Sign In';
    }
});
