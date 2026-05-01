import ImageViewer from "@/app/components/image-viewer/image-viewer";
import Grid from "@mui/material/Grid";
import IconButton from "@mui/material/IconButton";
import HomeIcon from '@mui/icons-material/Home';
import Link from "next/link";
import { imageUrl } from "@/app/api/mediaroot-server-api";
import { Media } from "@/app/types";
import { redirect } from "next/navigation";

export default async function ViewPage({ params }: { params: { id: string } }) {
  const base = process.env.NEXT_PUBLIC_MEDIA_API ?? "http://localhost:8080";
  const { id } = await params;

  let media: Media | null = null;
  try {
    const res = await fetch(`${base}/api/media/${id}`, { cache: 'no-store' });
    if (res.ok) media = await res.json();
  } catch {}

  if (!media) redirect('/');
  if (media.media_type === 'video') redirect(`/watch/${id}`);
  if (media.media_type === 'audio') redirect(`/listen/${id}`);

  return (
    <main>
      <Grid container spacing={1} sx={{ justifyContent: 'space-between', mt: 1, mb: 1, ml: 2, mr: 2 }}>
        <Link href={`/`}>
          <IconButton sx={{ width: 48, height: 48, bgcolor: "secondary.main", color: "primary.contrastText", m: .5 }}>
            <HomeIcon sx={{ width: 36, height: 36, color: 'video.contrastText' }} />
          </IconButton>
        </Link>
      </Grid>
      <Grid container sx={{ ml: 2, mr: 2, mb: 1, mt: 1, border: 'solid', borderColor: 'secondary.main', borderRadius: 1 }}>
        <Grid size='grow'>
          <ImageViewer
            src={imageUrl(id, media.added_at)}
            media={media}
          />
        </Grid>
      </Grid>
    </main>
  );
}