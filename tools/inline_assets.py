import os
import re
import gzip

with open('data/style.css', 'r', encoding='utf-8') as f:
    css = f.read().strip()

with open('data/app.js', 'r', encoding='utf-8') as f:
    js = f.read().strip()

with open('data/login.js', 'r', encoding='utf-8') as f:
    login_js = f.read().strip()

css_block = f"<style>\n{css}\n</style>"
js_block = f"<script>\n{js}\n</script>"
login_js_block = f"<script>\n{login_js}\n</script>"

# 1. Update index.html
with open('data/index.html', 'r', encoding='utf-8') as f:
    idx = f.read()

# Replace existing <style>...</style> or <link rel="stylesheet"...>
if '<style>' in idx and '</style>' in idx:
    idx = re.sub(r'<style>[\s\S]*?</style>', lambda m: css_block, idx, count=1)
elif '<link rel="stylesheet"' in idx:
    idx = re.sub(r'<link rel="stylesheet"[^>]*>', lambda m: css_block, idx, count=1)

# Replace existing <script>...</script> at bottom or <script src="/app.js"></script>
if '<script src="/app.js"></script>' in idx:
    idx = idx.replace('<script src="/app.js"></script>', js_block)
else:
    # Replace last <script> block before </body>
    idx = re.sub(r'<script>[\s\S]*?</script>\s*</body>', f"{js_block}\n</body>", idx, count=1)

with open('data/index.html', 'w', encoding='utf-8') as f:
    f.write(idx)

print(f"Updated data/index.html: {len(idx.encode('utf-8'))} bytes")

# 2. Update login.html
with open('data/login.html', 'r', encoding='utf-8') as f:
    login_html = f.read()

if '<style>' in login_html and '</style>' in login_html:
    login_html = re.sub(r'<style>[\s\S]*?</style>', lambda m: css_block, login_html, count=1)
elif '<link rel="stylesheet"' in login_html:
    login_html = re.sub(r'<link rel="stylesheet"[^>]*>', lambda m: css_block, login_html, count=1)

if '<script src="/login.js"></script>' in login_html:
    login_html = login_html.replace('<script src="/login.js"></script>', login_js_block)
else:
    login_html = re.sub(r'<script>[\s\S]*?</script>\s*</body>', f"{login_js_block}\n</body>", login_html, count=1)

with open('data/login.html', 'w', encoding='utf-8') as f:
    f.write(login_html)

print(f"Updated data/login.html: {len(login_html.encode('utf-8'))} bytes")

# 3. Generate GZIP compressed versions for lightning-fast LittleFS streaming
for fname in ['index.html', 'login.html', 'style.css', 'app.js', 'login.js']:
    fpath = os.path.join('data', fname)
    if os.path.exists(fpath):
        with open(fpath, 'rb') as f_in:
            raw_bytes = f_in.read()
        gz_bytes = gzip.compress(raw_bytes, compresslevel=9)
        gz_path = fpath + '.gz'
        with open(gz_path, 'wb') as f_out:
            f_out.write(gz_bytes)
        print(f"Compressed {fname} ({len(raw_bytes)} bytes) -> {fname}.gz ({len(gz_bytes)} bytes) [{100 - (len(gz_bytes)*100//len(raw_bytes))}% saved]")
