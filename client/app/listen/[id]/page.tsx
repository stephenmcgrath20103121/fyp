import AudioPlayer from "@/app/components/audio-player/audio-player";
import AlbumArt from "@/app/components/audio-player/album-art";
import Grid from "@mui/material/Grid";
import IconButton from "@mui/material/IconButton";
import HomeIcon from '@mui/icons-material/Home';
import Link from "next/link";
import placeholder from '@/public/albumArtPlaceholder.svg';
import { audioStreamUrl, thumbnailUrl } from "@/app/api/mediaroot-server-api";
import { Media } from "@/app/types/index"
import { redirect } from "next/navigation";

export default async function ListenPage({ params }: { params: { id: string } }) {
  const base = process.env.NEXT_PUBLIC_MEDIA_API ?? "http://localhost:8080";
  const { id } = await params;

  let media: Media | null = null;
  try {
    const res = await fetch(`${base}/api/media/${id}`, { cache: 'no-store' });
    if (res.ok) media = await res.json();
  } catch {}

  if (!media) redirect('/');
  if (media.media_type === 'video') redirect(`/watch/${id}`);
  if (media.media_type === 'image') redirect(`/view/${id}`);

  const albumArtSrc = media.thumbnail_path
    ? thumbnailUrl(media.id, media.added_at)
    : placeholder.src;

  return (
    <main>
      <Grid container spacing={1} sx={{ justifyContent: 'space-between', mt: 1, mb: 1, ml: 2, mr: 2 }}>
        <Link href={`/`}>
          <IconButton sx={{ width: 48, height: 48, bgcolor: "secondary.main", color: "primary.contrastText", m: .5 }}>
            <HomeIcon sx={{ width: 36, height: 36, color: 'video.contrastText' }} />
          </IconButton>
        </Link>
      </Grid>
      <Grid container sx={{justifyContent: 'center'}}>
        <Grid sx={{ ml: 2, mr: 2, mb: 1, mt: 4, maxWidth: 400, maxHeight: 400, border: 'solid', borderColor: 'secondary.main', bgcolor: 'primary.contrastText', borderRadius: 1 }}>
          <Grid container size='grow' sx={{justifyContent: 'center', p: 2}}>
            <AlbumArt
              src={albumArtSrc}
              alt={`Album art for ${media.title}`}
            />
          </Grid>
          <Grid container size='grow'  sx={{justifyContent: 'center', p: 1}}>
            <AudioPlayer
              src={audioStreamUrl(id)}
              storageKey={`media:${id}`}
            />
          </Grid>
        </Grid>
      </Grid>
    </main>
  );
}