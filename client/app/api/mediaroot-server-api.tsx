import { Media } from '@/app/types/index';

const API_BASE = process.env.NEXT_PUBLIC_MEDIA_API ?? 'http://localhost:8080';

//Helper function to reduce code reuse
async function jsonRequest<T>(
  path: string,
  init: RequestInit | undefined,
  fallbackMessage: string,
): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`, init);
  if (!response.ok) {
    let detail = '';
    try {
      const body = await response.json();
      if (body?.error) detail = `: ${body.error}`;
    } catch {
    }
    throw new Error(`${fallbackMessage}${detail}`);
  }
  return response.json() as Promise<T>;
}

//CRUD

//GET /api/media
export async function getMedia(): Promise<Media[]> {
  return jsonRequest<Media[]>('/api/media', undefined, 'Failed to fetch all Media');
}

//GET /api/media/{id}
export async function getMediaById(id: number): Promise<Media> {
  return jsonRequest<Media>(
    `/api/media/${id}`,
    undefined,
    `Failed to fetch media ${id}`,
  );
}

//POST /api/media
export async function uploadMedia(
    file: File,
    title?: string,
    onProgress?: (pct: number) => void,
): Promise<Media> {
    const url = `${process.env.NEXT_PUBLIC_MEDIA_API}/api/media`;

    const fd = new FormData();
    fd.append('file', file);
    if (title) fd.append('title', title);

    // Pick a media_type from the browser-provided MIME prefix.
    // Server currently only accepts "video"; the others are forward-compat.
    const mediaType = file.type.startsWith('audio/') ? 'audio'
                    : file.type.startsWith('image/') ? 'image'
                    : 'video';
    fd.append('media_type', mediaType);

    return new Promise<Media>((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        xhr.open('POST', url);

        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable && onProgress) {
                onProgress(Math.round((e.loaded / e.total) * 100));
            }
        };

        xhr.onload = () => {
            if (xhr.status >= 200 && xhr.status < 300) {
                try {
                    resolve(JSON.parse(xhr.responseText));
                } catch {
                    reject(new Error('Invalid response from server'));
                }
            } else {
                let msg = `Upload failed (${xhr.status})`;
                try {
                    const body = JSON.parse(xhr.responseText);
                    if (body?.error) msg = body.error;
                } catch { /* keep default */ }
                reject(new Error(msg));
            }
        };

        xhr.onerror = () => reject(new Error('Network error during upload'));
        xhr.onabort = () => reject(new Error('Upload aborted'));

        xhr.send(fd);
    });
}

//PUT /api/media/{id}
export async function updateMediaTitle(id: number, title: string): Promise<Media> {
  return jsonRequest<Media>(
    `/api/media/${id}`,
    {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ title }),
    },
    `Failed to update media ${id}`,
  );
}

//DELETE /api/media/{id}
export async function deleteMedia(id: number): Promise<{ result: string }> {
  return jsonRequest<{ result: string }>(
    `/api/media/${id}`,
    { method: 'DELETE' },
    `Failed to delete media ${id}`,
  );
}

//URL helpers
//GET /api/media/{id}/stream/playlist.m3u8
export function getStreamPlaylistUrl(id: number | string): string {
  return `${API_BASE}/api/media/${id}/stream/playlist.m3u8`;
}

//GET /api/media/{id}/stream/{segment}.ts
export function getStreamSegmentUrl(id: number | string, segment: string): string {
  return `${API_BASE}/api/media/${id}/stream/${segment}`;
}

//GET /api/media/{id}/thumbnail
export function getThumbnailUrl(id: number | string): string {
  return `${API_BASE}/api/media/${id}/thumbnail`;
}