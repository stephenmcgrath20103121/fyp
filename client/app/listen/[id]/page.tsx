import AudioPlayer from "@/app/components/audio-player/audio-player";
import Grid from "@mui/material/Grid";
import IconButton from "@mui/material/IconButton";
import HomeIcon from '@mui/icons-material/Home';
import Link from "next/link";
import Avatar from "@mui/material/Avatar";
import Image from 'next/image';
import img from '@/public/albumArtPlaceholder.svg';
import { audioStreamUrl } from "@/app/api/mediaroot-server-api";
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

  return (
    <main>
      <Grid container spacing={1} sx={{ justifyContent: 'space-between', mt: 1, mb: 1, ml: 2, mr: 2 }}>
        <Link href={`/`}>
          <IconButton sx={{ width: 48, height: 48, bgcolor: "secondary.main", color: "primary.contrastText", m: .5 }}>
            <HomeIcon sx={{ width: 36, height: 36, color: 'video.contrastText' }} />
          </IconButton>
        </Link>
      </Grid>
      <Grid sx={{ ml: 16, mr: 16, mb: 1, mt: 4, border: 'solid', borderColor: 'secondary.main', bgcolor: 'primary.contrastText', borderRadius: 1 }}>
        <Grid container size='grow' sx={{justifyContent: 'center', p: 2}}>
          <Avatar sx={{bgcolor: 'white', width: 250, height: 250}}>
          <Image
            src={img}
            style={{width: 200, height: 200}}
            alt="Album Art Placeholder"
          />
          </Avatar>
        </Grid>
        <Grid container size='grow'  sx={{justifyContent: 'center', p: 1}}>
          <AudioPlayer
            src={audioStreamUrl(id)}
            storageKey={`media:${id}`}
          />
        </Grid>
      </Grid>
    </main>
  );
}