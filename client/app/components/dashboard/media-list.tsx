'use client'
import * as React from 'react';
import Typography from '@mui/material/Typography';
import Grid from '@mui/material/Grid';

import MediaCard from "./media-card"

export default function MediaList() {
  const [selectedIndex, setSelectedIndex] = React.useState(1);

  const handleListItemClick = (
    event: React.MouseEvent<HTMLDivElement, MouseEvent>,
    index: number,
  ) => {
    setSelectedIndex(index);
  };
  
  return (
    <Grid sx={{ maxWidth: 750, bgcolor: 'background.paper', pl: 1, pr: 1, p1: 2, borderRadius: 3}}>
      <Typography component ="h5" variant="h5" gutterBottom sx={{pl:2, pt:2.5, pb:.5}}>
        Media
      </Typography>
      <Grid sx={{ maxWidth: 700, bgcolor: 'tertiary.main', margin: 'auto', borderRadius: 2 }}>
      <Grid container sx={{justifyContent: "space-evenly", alignItems: "flex-start",}}>
        <MediaCard />
        <MediaCard />
        <MediaCard />
        <MediaCard />
        <MediaCard />
        <MediaCard />
      </Grid>
      </Grid>
    </Grid>
  );
}