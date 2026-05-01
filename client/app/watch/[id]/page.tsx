import VideoPlayer from "@/app/components/video-player/video-player";
import Grid from "@mui/material/Grid";
import IconButton from "@mui/material/IconButton"
import HomeIcon from '@mui/icons-material/Home';
import Link from "next/link";
import { getStreamPlaylistUrl, thumbnailUrl } from '@/app/api/mediaroot-server-api';
import { Media } from '@/app/types/index';

export default async function WatchPage({ params }: { params: { id: string } }) {
  const base = process.env.NEXT_PUBLIC_MEDIA_API ?? "http://localhost:8080";
  const { id } = await params

  let addedAt: string | undefined;
  try {
    const res = await fetch(`${base}/api/media/${id}`, { cache: 'no-store' });
    if (res.ok) {
      const m: Media = await res.json();
      addedAt = m.added_at;
    }
  } catch {}

  return (
    <main>
      <Grid container spacing={1} sx={{justifyContent: 'space-between', mt: 1, mb: 1, ml: 2, mr: 2}}>
        <Link href={`/`}>
          <IconButton sx={{width: 48, height: 48, bgcolor: "secondary.main", color: "primary.contrastText", m: .5}}>
            <HomeIcon sx={{width: 36, height: 36, color: 'video.contrastText'}} />
          </IconButton>
        </Link>
      </Grid>
      <Grid container sx={{ ml: 2, mr: 2, mb: 1, mt: 1, border: 'solid', borderColor: 'secondary.main', borderRadius: 1}}>
        <Grid size='grow'>
          <VideoPlayer
            src={getStreamPlaylistUrl(id)}
            poster={thumbnailUrl(id, addedAt)}
            storageKey={`media:${id}`}
          />
        </Grid>
      </Grid>
    </main>
  );
}