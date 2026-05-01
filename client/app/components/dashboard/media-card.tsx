'use client'
import React from "react";
import Card from "@mui/material/Card";
import CardActions from "@mui/material/CardActions";
import CardContent from "@mui/material/CardContent";
import CardMedia from "@mui/material/CardMedia";
import CardHeader from "@mui/material/CardHeader";
import Button from "@mui/material/Button";
import Typography from "@mui/material/Typography";
import FolderIcon from "@mui/icons-material/Folder";
import TimerIcon from "@mui/icons-material/Timer";
import DeleteIcon from '@mui/icons-material/Delete';
import EditIcon from '@mui/icons-material/Edit';
import PlayIcon from '@mui/icons-material/PlayCircle';
import Grid from "@mui/material/Grid";
import Link from "next/link";
import { Media } from '@/app/types/index';
import { secsToMins } from '@/app/lib/utils';
import { thumbnailUrl } from '@/app/api/mediaroot-server-api';
import DeleteMediaDialog from '@/app/components/dashboard/delete-media-dialog';
import EditMediaDialog from '@/app/components/dashboard/edit-media-dialog';
import placeholder from '@/public/placeholder.svg';

function viewerPathFor(mediaType: string, id: number): string {
  switch (mediaType) {
    case 'audio': return `/listen/${id}`;
    case 'image': return `/view/${id}`;
    case 'video':
    default:      return `/watch/${id}`;
  }
}

export default function MediaCard({ media }: { media: Media }) {
  const [deleteDialogOpen, setDeleteDialogOpen] = React.useState(false);
  const [editDialogOpen, setEditDialogOpen] = React.useState(false);
  return (
    <Card sx={{ maxWidth: 200, maxHeight: 500, mt: 1.5, mb: 1.5, bgcolor: 'primary.main' }}>
      <CardHeader
        title={
          <Typography variant="h6" component="p" sx={{ color: 'primary.contrastText', maxWidth: 250, mt:1 }}>
            {media.title}{" "}
          </Typography>
        }
        sx={{ height: 140, color: 'tertiary.contrastText'}}
      />

      <CardMedia
        component="img"
        src={media.thumbnail_path ? thumbnailUrl(media.id, media.added_at) : placeholder.src}
        onError={(e) => { (e.currentTarget as HTMLImageElement).src = placeholder.src; }}
        sx={{ width: 150, height: 150, ml: 3, mr: 3, mb: .5, border: 'solid', borderColor: 'tertiary.main', borderRadius: 1, objectFit: 'cover' }}
      />
      <CardContent sx={{bgcolor: 'tertiary.light'}} >
        <Grid container sx={{bgcolor: 'primary.main', color: 'primary.contrastText', borderRadius: 2, pl: 1, pr: 1, pt: .5}}>
          {media.media_type !== 'image' && (
          <Grid size={{xs: 8}} sx={{mb:1}}>
            <Typography variant="body1" component="p" sx={{fontSize: 12}}>
              <TimerIcon sx={{maxWidth: 14, maxHeight: 14, mr: .5, mb: .5}} />
              {secsToMins(media.duration_secs)}
            </Typography>
          </Grid>
          )}
        </Grid>
      </CardContent>
      <CardActions disableSpacing>
        <Grid container>
          <Grid>
            <Link href={viewerPathFor(media.media_type, media.id)}>
              <Button variant="contained" size="small" startIcon={<PlayIcon />} sx={{bgcolor: "secondary.main", color: "primary.contrastText", ml: .5, mr: .5, mb: 1, pl: 7.5, pr: 7.5}}>
                View
              </Button>
            </Link>
          </Grid>
          <Grid container size='grow' sx={{justifyContent: 'space-evenly'}}>
            <Button variant="contained" size="small" startIcon={<EditIcon />} aria-label={`Edit ${media.title}`} onClick={() => setEditDialogOpen(true)} sx={{bgcolor: "secondary.main", color: "primary.contrastText", ml: .5, mr: .5, mb: 1, pl: 1.3, pr: 1.3}}>
              Edit
            </Button>
            <Button variant="contained" size="small" startIcon={<DeleteIcon />} aria-label={`Delete ${media.title}`} onClick={() => setDeleteDialogOpen(true)} sx={{bgcolor: "secondary.main", color: "primary.contrastText", ml: .5, mr: .5, mb: 1, pl: 1.5, pr: 1.25}}>
              Delete
            </Button>
          </Grid>
        </Grid>
      </CardActions>
      <DeleteMediaDialog
        open={deleteDialogOpen}
        media={media}
        onClose={() => setDeleteDialogOpen(false)}
      />
      <EditMediaDialog
        open={editDialogOpen}
        media={media}
        onClose={() => setEditDialogOpen(false)}
      />
    </Card>
  );
}