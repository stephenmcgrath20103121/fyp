'use client'
import * as React from 'react';
import Typography from '@mui/material/Typography';
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

export default function SideMenu() {
  const [selectedIndex, setSelectedIndex] = React.useState(0);

  const handleListItemClick = (
    event: React.MouseEvent<HTMLDivElement, MouseEvent>,
    index: number,
  ) => {
    setSelectedIndex(index);
  };
  
  return (
    <Grid sx={{ maxWidth: 350, bgcolor: 'background.paper', p: 1, borderRadius: 3}}>
      <Typography component ="h5" variant="h5" gutterBottom sx={{pl:2, pt:1, pb:.5}}>
        Library
      </Typography>
      <Grid sx={{ maxWidth: 300, bgcolor: 'primary.main', margin: 2, borderRadius: 2 }}>
      <nav aria-label="media types">
        <List>
          <ListItem  alignItems="flex-start">
            <ListItemButton 
              sx={{ maxWidth: 300, bgcolor: 'primary.main', '&:hover': {bgcolor: 'video.light'}, color: 'video.contrastText', borderRadius: 2 }}
              selected={selectedIndex === 0}
              onClick={(event) => handleListItemClick(event, 0)}
            >
              <ListItemAvatar>
                <Avatar sx={{bgcolor: 'video.main'}}>
                  <VideoIcon sx={{color: 'video.contrastText'}} />
                </Avatar>
              </ListItemAvatar>
              <ListItemText primary="Videos" />
            </ListItemButton>
          </ListItem>
          <Divider aria-hidden="true" variant="middle" component="li" sx={{color: 'divider'}} />
          <ListItem alignItems="flex-start">
            <ListItemButton 
              sx={{ width: '100%', maxWidth: 300, bgcolor: 'primary.main', '&:hover': {bgcolor: 'music.light'}, color: 'music.contrastText', borderRadius: 2 }}
              selected={selectedIndex === 1}
              onClick={(event) => handleListItemClick(event, 1)}
            >
              <ListItemAvatar>
                <Avatar sx={{bgcolor: 'music.main'}}>
                  <MusicIcon sx={{color: 'music.contrastText'}} />
                </Avatar>
              </ListItemAvatar>
              <ListItemText primary="Music" />
            </ListItemButton>
          </ListItem>
          <Divider aria-hidden="true" variant="middle" component="li" sx={{color: 'divider'}} />
          <ListItem alignItems="flex-start">
            <ListItemButton 
              sx={{ width: '100%', maxWidth: 300, bgcolor: 'primary.main', '&:hover': {bgcolor: 'image.light'}, color: 'image.contrastText', borderRadius: 2 }}
              selected={selectedIndex === 2}
              onClick={(event) => handleListItemClick(event, 2)}
            >
              <ListItemAvatar>
                <Avatar sx={{bgcolor: 'image.main'}}>
                  <ImageIcon sx={{color: 'image.contrastText'}} />
                </Avatar>
              </ListItemAvatar>
              <ListItemText primary="Images" />
            </ListItemButton>
          </ListItem>
        </List>
      </nav>
      </Grid>
    </Grid>
  );
}