'use client'
import * as React from 'react';
import Typography from '@mui/material/Typography';
import Box from '@mui/material/Box';
import Grid from '@mui/material/Grid';
import List from '@mui/material/List';
import ListItem from '@mui/material/ListItem';
import ListItemButton from '@mui/material/ListItemButton';
import ListItemText from '@mui/material/ListItemText';
import VideoIcon from '@mui/icons-material/Movie';
import MusicIcon from '@mui/icons-material/MusicNote';
import ImageIcon from '@mui/icons-material/Image';
import ListItemAvatar from '@mui/material/ListItemAvatar';
import Avatar from '@mui/material/Avatar';
import Divider from '@mui/material/Divider';

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
    <Box sx={{ width: '100%', maxWidth: 750, bgcolor: 'background.paper', pl: 1, pr: 1, p1: 2, borderRadius: 3}}>
      <Typography component ="h5" variant="h5" gutterBottom sx={{pl:2, pt:2.5, pb:.5}}>
        Media
      </Typography>
      <Box sx={{ width: '100%', maxWidth: 700, bgcolor: 'tertiary.main', margin: 'auto', borderRadius: 2 }}>
      <Grid container sx={{justifyContent: "space-evenly", alignItems: "flex-start",}}>
        <MediaCard />
        <MediaCard />
        <MediaCard />
        <MediaCard />
        <MediaCard />
        <MediaCard />
      </Grid>
      </Box>
    </Box>
  );
}