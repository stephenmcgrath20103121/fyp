import VideoPlayer from "@/app/components/video-player/video-player";
import Grid from "@mui/material/Grid";
import IconButton from "@mui/material/IconButton"
import HomeIcon from '@mui/icons-material/Home';
import Link from "next/link";

export default async function WatchPage({ params }: { params: { id: string } }) {
  const base = process.env.NEXT_PUBLIC_MEDIA_API ?? "http://localhost:8080";
  const { id } = await params
  return (
    <main>
      <Grid container spacing={1} sx={{justifyContent: 'space-between', mt: 1, mb: 1, ml: 2, mr: 2}}>
        <Link href={`/`}>
          <IconButton sx={{width: 32, height: 32, bgcolor: "secondary.main", color: "primary.contrastText", m: .5}}>
            <HomeIcon sx={{width: 24, height: 24, color: 'video.contrastText'}} />
          </IconButton>
        </Link>
      </Grid>
      <Grid container sx={{ ml: 2, mr: 2, mb: 1, mt: 1, border: 'solid', borderColor: 'secondary.main', borderRadius: 1}}>
        <Grid size='grow'>
          <VideoPlayer
            src={`${base}/api/media/${id}/stream/playlist.m3u8`}
            poster={`${base}/api/media/${id}/thumbnail`}
            storageKey={`media:${id}`}
          />
        </Grid>
      </Grid>
    </main>
  );
}