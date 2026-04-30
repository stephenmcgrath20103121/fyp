'use client'
import * as React from 'react';
import Typography from '@mui/material/Typography';
import Grid from '@mui/material/Grid';
import IconButton from "@mui/material/IconButton";
import CancelIcon from "@mui/icons-material/CancelOutlined";
import Search from '@/app/components/dashboard/search';
import MediaCard from "@/app/components/dashboard/media-card";
import Spinner from '@/app/components/dashboard/spinner';
import { useMedia } from '@/app/hooks/useMedia';

export default function MediaList() {
  const { data, error, isLoading, isError }  = useMedia();

  if (isLoading) { return <Spinner /> }
  if (isError) { return <Typography sx={{color: 'error.main'}}>{error.message}</Typography> }
  if (!data) { return null }

  const mediaCards = data.map((m) => (
    <MediaCard key={m.id} media={m} />
  ));

  return (
    <Grid sx={{ bgcolor: 'background.paper', pl: 1, pr: 1, pt: 1, pb: 2, borderRadius: 3}}>
      <Grid container columnSpacing={2}>
        <Typography component ="h5" variant="h5" gutterBottom sx={{pl:2, pt:2.5, pb:.5}}>
          Media
        </Typography>
        <Grid size='grow'>
          <Search />
        </Grid>
        <IconButton sx={{width: 42, height: 42, color: "primary.contrastText", mt: 2, mr: 1}}>
          <CancelIcon sx={{width: 42, height: 42, color: 'primary.dark'}} />
        </IconButton>
      </Grid>
      <Grid sx={{ bgcolor: 'tertiary.main', margin: 'auto', borderRadius: 2 }}>
      <Grid container sx={{justifyContent: "space-evenly", alignItems: "flex-start",}}>
        {mediaCards}
      </Grid>
      </Grid>
    </Grid>
  );
}