const grid = document.querySelector('#gallery-grid');
const input = document.querySelector('#file-input');
const notice = document.querySelector('#notice');
const dialog = document.querySelector('#photo-dialog');
let captures = [];
let selectedCapture = null;

const formatBytes = (bytes) => {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(0)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
};
const formatDate = (value, options = {}) => new Intl.DateTimeFormat(undefined, options).format(new Date(value));
const escapeHtml = (value) => String(value).replace(/[&<>"']/g, (char) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));

function showNotice(message, success = false) {
  notice.textContent = message;
  notice.className = `notice${success ? ' success' : ''}`;
  notice.hidden = false;
  window.setTimeout(() => { notice.hidden = true; }, 5500);
}

function render() {
  const query = document.querySelector('#search-input').value.trim().toLowerCase();
  const sort = document.querySelector('#sort-select').value;
  const shown = captures.filter((item) => `${item.original_name} ${item.filename}`.toLowerCase().includes(query));
  shown.sort((a, b) => sort === 'newest' ? b.captured_at.localeCompare(a.captured_at) : a.captured_at.localeCompare(b.captured_at));
  document.querySelector('#capture-count').textContent = captures.length;
  document.querySelector('#heading-count').textContent = captures.length;
  document.querySelector('#side-count').textContent = captures.length;
  document.querySelector('#storage-size').textContent = formatBytes(captures.reduce((sum, item) => sum + item.size_bytes, 0));
  document.querySelector('#latest-capture').textContent = captures.length ? formatDate(captures[0].captured_at, {month:'short', day:'numeric'}) : '—';
  document.querySelector('#empty-state').hidden = captures.length > 0;
  document.querySelector('#no-results').hidden = captures.length === 0 || shown.length > 0;
  grid.innerHTML = shown.map((item) => {
    const title = item.original_name || item.filename;
    return `<article class="photo-card" tabindex="0" data-id="${escapeHtml(item.id)}" aria-label="Open ${escapeHtml(title)}">
      <div class="photo-thumb"><img loading="lazy" src="${escapeHtml(item.url)}" alt="${escapeHtml(title)}"><span class="photo-type">JPEG</span></div>
      <div class="photo-info"><div style="min-width:0"><div class="photo-name">${escapeHtml(title)}</div><div class="photo-sub">${formatDate(item.captured_at,{month:'short',day:'numeric',year:'numeric'})} · ${formatDate(item.captured_at,{hour:'numeric',minute:'2-digit'})}</div></div><span class="photo-size">${formatBytes(item.size_bytes)}</span></div>
    </article>`;
  }).join('');
  document.querySelectorAll('.photo-card').forEach((card) => {
    card.addEventListener('click', () => openCapture(card.dataset.id));
    card.addEventListener('keydown', (event) => { if (event.key === 'Enter' || event.key === ' ') openCapture(card.dataset.id); });
  });
}

async function refresh() {
  try {
    const response = await fetch('/api/captures', {cache:'no-store'});
    if (!response.ok) throw new Error('Could not load your library.');
    const data = await response.json();
    captures = data.captures || [];
    document.querySelector('#connection-dot').style.background = '#7ea985';
    document.querySelector('#connection-text').textContent = 'Laptop ready';
    document.querySelector('.connection').classList.remove('offline');
    document.querySelector('#device-status').textContent = captures.length ? 'Photos arriving' : 'Ready for first photo';
    document.querySelector('#device-dot').style.background = captures.length ? '#7ea985' : '#d9ae6e';
    render();
  } catch (error) {
    document.querySelector('#connection-text').textContent = 'Offline';
    document.querySelector('.connection').classList.add('offline');
    document.querySelector('#device-status').textContent = 'Not connected';
    showNotice('Can’t reach the DIGICAM server. Check that it is running, then refresh this page.');
  }
}

function openCapture(id) {
  selectedCapture = captures.find((item) => item.id === id);
  if (!selectedCapture) return;
  document.querySelector('#dialog-image').src = selectedCapture.url;
  document.querySelector('#dialog-title').textContent = selectedCapture.original_name || selectedCapture.filename;
  document.querySelector('#dialog-date').textContent = formatDate(selectedCapture.captured_at,{weekday:'long',month:'long',day:'numeric',year:'numeric',hour:'numeric',minute:'2-digit'});
  document.querySelector('#dialog-metadata').innerHTML = `<span>${selectedCapture.width} × ${selectedCapture.height}</span><span>${formatBytes(selectedCapture.size_bytes)}</span><span>JPEG</span>`;
  document.querySelector('#download-link').href = selectedCapture.url;
  dialog.showModal();
}

async function uploadFiles(files) {
  const valid = Array.from(files).filter((file) => file.type === 'image/jpeg' || /\.jpe?g$/i.test(file.name));
  if (!valid.length) return showNotice('Choose a JPEG image to add.');
  for (const file of valid) {
    if (file.size > 16 * 1024 * 1024) { showNotice(`${file.name} is larger than 16 MB.`); continue; }
    const body = new FormData();
    body.append('image', file);
    try {
      const response = await fetch('/api/captures', {method:'POST',body});
      const result = await response.json();
      if (!response.ok) throw new Error(result.error || 'Upload failed.');
    } catch (error) { showNotice(`${file.name}: ${error.message}`); return; }
  }
  await refresh();
  showNotice(valid.length === 1 ? 'Photo added to your library.' : `${valid.length} photos added to your library.`, true);
}

document.querySelector('#upload-button').addEventListener('click', () => input.click());
document.querySelector('#empty-upload').addEventListener('click', () => input.click());
input.addEventListener('change', () => { uploadFiles(input.files); input.value = ''; });
document.querySelector('#search-input').addEventListener('input', render);
document.querySelector('#sort-select').addEventListener('change', render);
document.querySelector('#dialog-close').addEventListener('click', () => dialog.close());
dialog.addEventListener('click', (event) => { if (event.target === dialog) dialog.close(); });
document.querySelector('#delete-button').addEventListener('click', async () => {
  if (!selectedCapture || !window.confirm('Delete this photo from your laptop? This can’t be undone.')) return;
  try {
    const response = await fetch(`/api/captures/${encodeURIComponent(selectedCapture.id)}`, {method:'DELETE'});
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || 'Delete failed.');
    dialog.close();
    await refresh();
    showNotice('Photo deleted from your library.', true);
  } catch (error) { showNotice(error.message); }
});

const dropzone = document.querySelector('#dropzone');
window.addEventListener('dragenter', (event) => { if (event.dataTransfer?.types.includes('Files')) dropzone.hidden = false; });
dropzone.addEventListener('dragover', (event) => event.preventDefault());
dropzone.addEventListener('dragleave', (event) => { if (!dropzone.contains(event.relatedTarget)) dropzone.hidden = true; });
dropzone.addEventListener('drop', (event) => { event.preventDefault(); dropzone.hidden = true; uploadFiles(event.dataTransfer.files); });
window.addEventListener('dragover', (event) => { if (event.dataTransfer?.types.includes('Files')) event.preventDefault(); });
window.addEventListener('drop', (event) => { if (!dropzone.contains(event.target) && event.dataTransfer?.types.includes('Files')) { event.preventDefault(); uploadFiles(event.dataTransfer.files); } });
refresh();
window.setInterval(refresh, 10000);
