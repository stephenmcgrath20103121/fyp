'use client'
import * as React from 'react';
import { useQuery } from '@tanstack/react-query';
import Typography from '@mui/material/Typography';
import Grid from '@mui/material/Grid';
import IconButton from "@mui/material/IconButton";
import CancelIcon from "@mui/icons-material/CancelOutlined";
import Search from '@/app/components/dashboard/search';
import MediaCard from "@/app/components/dashboard/media-card";
import { Media } from '@/app/types/index';
import Spinner from '@/app/components/dashboard/spinner';
import { getMedia } from '@/app/api/mediaroot-server-api';

export default function MediaList() {
  const [selectedIndex, setSelectedIndex] = React.useState(1);

  const handleListItemClick = (
    event: React.MouseEvent<HTMLDivElement, MouseEvent>,
    index: number,
  ) => {
    setSelectedIndex(index);
  };

  const { data, error, isLoading, isError }  = useQuery<Media[]>({
    queryKey: ['media'], 
    queryFn: async () => {
      const results = await fetch (`${process.env.NEXT_PUBLIC_MEDIA_API}/api/media`);
      if(!results.ok) throw new Error('Failed to fetch your Media');
      return results.json();
    },
  })

  if (isLoading) { return <Spinner /> }
  if (isError) { return <Typography sx={{color: 'error.main'}}>{error.message}</Typography> }
  if (!data) { return null }

  let mediaCards = data.map((m) => (
    <MediaCard key={m.id} media={m} />
  ));

  return (
    <Grid sx={{ bgcolor: 'background.paper', pl: 1, pr: 1, p1: 2, borderRadius: 3}}>
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